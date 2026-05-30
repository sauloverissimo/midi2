/*
 * Copyright (c) 2026 Saulo Verissimo
 * SPDX-License-Identifier: MIT
 *
 * midi2 Zephyr example: full-spec USB MIDI 2.0 device showcase on
 * the Raspberry Pi Pico (RP2040), headless. Demonstrates broad spec
 * coverage from M2-104-UM v1.1.2 by emitting a 15-scene cycle.
 *
 * Layering:
 *   Zephyr usbd_midi2     - USB enumeration, Endpoint Discovery
 *                           (CONFIG_MIDI2_UMP_STREAM_RESPONDER reads
 *                           the zephyr,midi2-device DT node)
 *   midi2 (C99)           - typed dispatch, message construction,
 *                           SysEx7 reassembly, MIDI-CI responder
 *   This main.c           - scene orchestration, JR heartbeat,
 *                           identity bootstrap
 *
 * Scene cycle (~30 s):
 *   A - Flex Data Setup:    Tempo, Time Sig, Key Sig, Metronome,
 *                            Chord Name (Cmaj7), Start of Clip
 *   B - Per-Note expression: Per-Note Pitch Bend vibrato, Registered +
 *                            Assignable Per-Note Controllers,
 *                            Per-Note Management Reset
 *   C - Resolution showcase: 16-bit velocity, 32-bit CC, PB, Poly
 *                            Pressure on a chromatic walk
 *   D - Program Change with Bank MSB/LSB in a single UMP
 *   E - RPN/NRPN 32-bit + Relative RPN/NRPN
 *   F - Note On with Attribute pitch_7_9 (microtonal +50 cents)
 *   G - SysEx7 Universal Identity Reply (Start+End) + a 3-packet
 *        bulk message exercising the Continue status (MT 0x3 0x20)
 *   H - Delta Clockstamp + DCTPQ
 *   I - PE Notify (broadcast OverlayRate to subscribers)
 *   J - End of Clip
 *   K - MIDI 1.0 Channel Voice burst (MT 0x2)
 *   L - System Common + Real-Time burst (MT 0x1)
 *   M - Flex Metadata Text (project name + composer)
 *   N - Utility additions (Noop + JR Clock)
 *
 * Always on while mounted:
 *   - JR Timestamp heartbeat every 500 ms (MT 0x0)
 *   - LED follows USB-ready state
 *   - MIDI-CI Discovery + Profile + 3 PE properties + collision-safe MUID
 *
 * USB identity: VID 0x2FE3 / PID 0x40A0. Zephyr project VID is used
 * for development; shipped products allocate their own via USB-IF or
 * pid.codes.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_midi2.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(midi2_showcase, LOG_LEVEL_INF);

#include "midi2_msg.h"
#include "midi2_dispatch.h"
#include "midi2_proc.h"
#include "midi2_ci.h"

/*--------------------------------------------------------------------+
 * Identity
 *--------------------------------------------------------------------*/

#define MIDI2_VID            0x2FE3
#define MIDI2_PID            0x40A0
#define MIDI2_MFR_STR        "github.com/sauloverissimo"
#define MIDI2_PRODUCT_STR    "midi2 RP2040 Showcase"

static const uint8_t  kProfileId[5] = {0x7D, 0x00, 0x00, 0x01, 0x00};

/* MFR ID is the 24-bit USB MIDI / SysEx Universal-NonRealtime prefix.
 * 0x7D = educational / private use. */
#define MIDI2_CI_MFR        0x7D0000u
#define MIDI2_CI_FAMILY     0x0001
#define MIDI2_CI_MODEL      0x0001
#define MIDI2_CI_VERSION    0x00050000u

/* Mutable, advertised via PE OverlayRate. Bumped each cycle. */
static char g_overlay_rate[40] = "{\"rateHz\":50}";

/*--------------------------------------------------------------------+
 * Devicetree handles
 *--------------------------------------------------------------------*/

static const struct device *const midi_dev = DEVICE_DT_GET(DT_NODELABEL(usb_midi));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

/*--------------------------------------------------------------------+
 * USB device context (inline minimal init)
 *--------------------------------------------------------------------*/

USBD_DEVICE_DEFINE(midi2_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   MIDI2_VID, MIDI2_PID);

USBD_DESC_LANG_DEFINE(midi2_lang);
USBD_DESC_MANUFACTURER_DEFINE(midi2_mfr, MIDI2_MFR_STR);
USBD_DESC_PRODUCT_DEFINE(midi2_product, MIDI2_PRODUCT_STR);
USBD_DESC_CONFIG_DEFINE(midi2_cfg_desc, "FS Configuration");

USBD_CONFIGURATION_DEFINE(midi2_fs_config, USB_SCD_SELF_POWERED, 200,
			  &midi2_cfg_desc);

static int midi2_usbd_setup(void)
{
	int err;

	if ((err = usbd_add_descriptor(&midi2_usbd, &midi2_lang)))      return err;
	if ((err = usbd_add_descriptor(&midi2_usbd, &midi2_mfr)))       return err;
	if ((err = usbd_add_descriptor(&midi2_usbd, &midi2_product)))   return err;

	if ((err = usbd_add_configuration(&midi2_usbd, USBD_SPEED_FS,
					  &midi2_fs_config)))           return err;
	if ((err = usbd_register_all_classes(&midi2_usbd, USBD_SPEED_FS, 1,
					     NULL)))                    return err;

	usbd_device_set_code_triple(&midi2_usbd, USBD_SPEED_FS,
				    USB_BCC_MISCELLANEOUS, 0x02, 0x01);

	if ((err = usbd_init(&midi2_usbd))) return err;
	return usbd_enable(&midi2_usbd);
}

/*--------------------------------------------------------------------+
 * UMP transport bridge: midi2 -> Zephyr usbd_midi_send
 *--------------------------------------------------------------------*/

static atomic_t g_midi_ready;

static void send_ump(const uint32_t *words, uint32_t count)
{
	if (!atomic_get(&g_midi_ready)) {
		return;
	}
	struct midi_ump ump = {0};
	if (count > 4) { count = 4; }
	memcpy(ump.data, words, count * sizeof(uint32_t));
	(void)usbd_midi_send(midi_dev, ump);
}

/* midi2_ci write callback. Returns count on send (the proc layer
 * treats nonzero as "accepted"). */
static uint32_t ci_write_fn(const uint32_t *words, uint32_t count, void *ctx)
{
	ARG_UNUSED(ctx);
	send_ump(words, count);
	return count;
}

/*--------------------------------------------------------------------+
 * Inbound UMP dispatch + SysEx7 reassembly for MIDI-CI
 *--------------------------------------------------------------------*/

static midi2_dispatch    g_dp;
static midi2_proc_state  g_proc;
static uint8_t           g_sysex7_buf[256];

static midi2_ci_state    g_ci;
static uint8_t           g_ci_profiles[4][5];
static midi2_ci_property g_ci_props[4];

/* Forward sysex7-complete chunks to the MIDI-CI responder. */
static void on_sysex7_complete(uint8_t group, const uint8_t *data,
			       uint16_t len, void *ctx)
{
	ARG_UNUSED(ctx);
	(void)midi2_ci_process_sysex(&g_ci, group, data, len);
}

/* Telemetry callbacks for the dispatcher LOG trail. */
static void on_note_on_cb(uint8_t group, uint8_t channel, uint8_t note,
			  uint16_t velocity, uint8_t attr_type,
			  uint16_t attr_data, void *ctx)
{
	ARG_UNUSED(ctx); ARG_UNUSED(attr_type); ARG_UNUSED(attr_data);
	LOG_INF("RX NoteOn  g=%u ch=%u n=%u v=%u",
		group, channel, note, velocity);
}

static void on_cc_cb(uint8_t group, uint8_t channel, uint8_t index,
		     uint32_t value, void *ctx)
{
	ARG_UNUSED(ctx);
	LOG_INF("RX CC      g=%u ch=%u idx=%u val=0x%08x",
		group, channel, index, value);
}

static void on_program_cb(uint8_t group, uint8_t channel, uint8_t program,
			  bool bank_valid, uint8_t bank_msb, uint8_t bank_lsb,
			  void *ctx)
{
	ARG_UNUSED(ctx);
	LOG_INF("RX Program g=%u ch=%u p=%u bank=%s msb=%u lsb=%u",
		group, channel, program, bank_valid ? "valid" : "off",
		bank_msb, bank_lsb);
}

static void rx_packet_cb(const struct device *dev, const struct midi_ump ump)
{
	ARG_UNUSED(dev);
	uint32_t n = UMP_NUM_WORDS(ump);

	/* All inbound UMPs go through midi2_proc: it forwards plain
	 * messages straight to dispatch and reassembles fragmented SysEx
	 * before invoking on_sysex7_complete (which routes to the CI
	 * responder). */
	midi2_proc_feed(&g_proc, ump.data, n);
}

/*--------------------------------------------------------------------+
 * USB lifecycle
 *--------------------------------------------------------------------*/

static void midi_ready_cb(const struct device *dev, const bool ready)
{
	ARG_UNUSED(dev);
	atomic_set(&g_midi_ready, ready ? 1 : 0);
	if (led.port) {
		gpio_pin_set_dt(&led, ready ? 1 : 0);
	}
	LOG_INF("USB MIDI 2.0 %s", ready ? "ready" : "suspended");
}

static const struct usbd_midi_ops midi_ops = {
	.rx_packet_cb = rx_packet_cb,
	.ready_cb     = midi_ready_cb,
};

/*--------------------------------------------------------------------+
 * JR Heartbeat
 *--------------------------------------------------------------------*/

static void jr_heartbeat_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	if (!atomic_get(&g_midi_ready)) { return; }
	uint32_t w = midi2_msg_jr_timestamp(15625u);  /* 500 ms in 1/31250s */
	send_ump(&w, 1);
}

K_TIMER_DEFINE(jr_heartbeat_timer, jr_heartbeat_handler, NULL);

/*--------------------------------------------------------------------+
 * MIDI-CI bootstrap
 *--------------------------------------------------------------------*/

static const char *overlay_rate_get(const char *name, void *ctx)
{
	ARG_UNUSED(name); ARG_UNUSED(ctx);
	return g_overlay_rate;
}

static bool overlay_rate_set(const char *name, const char *value, void *ctx)
{
	ARG_UNUSED(name); ARG_UNUSED(ctx);
	if (value == NULL) { return false; }
	strncpy(g_overlay_rate, value, sizeof(g_overlay_rate) - 1);
	g_overlay_rate[sizeof(g_overlay_rate) - 1] = '\0';
	LOG_INF("CI OverlayRate <- %s", g_overlay_rate);
	return true;
}

static const char *channel_list_get(const char *name, void *ctx)
{
	ARG_UNUSED(name); ARG_UNUSED(ctx);
	return "{\"channels\":[0,1,2,3]}";
}

static uint32_t ci_rng(void *ctx)
{
	ARG_UNUSED(ctx);
	return sys_rand32_get();
}

static void install_ci_bootstrap(void)
{
	int rc;

	midi2_ci_init(&g_ci, sys_rand32_get(),
		      g_ci_profiles, ARRAY_SIZE(g_ci_profiles),
		      g_ci_props,    ARRAY_SIZE(g_ci_props));
	midi2_ci_set_rng(&g_ci, ci_rng, NULL);
	midi2_ci_set_write_fn(&g_ci, ci_write_fn, NULL);
	midi2_ci_set_identity(&g_ci, MIDI2_CI_MFR, MIDI2_CI_FAMILY,
			      MIDI2_CI_MODEL, MIDI2_CI_VERSION);
	midi2_ci_set_nak_on_unknown(&g_ci, true);

	rc = midi2_ci_add_profile(&g_ci, kProfileId);
	LOG_INF("CI addProfile rc=%d", rc);

	rc = midi2_ci_add_property_static(&g_ci, "DeviceInfo",
		"{\"manufacturer\":\"github.com/sauloverissimo\","
		 "\"family\":\"rpi-pico-device-midi2\","
		 "\"model\":\"showcase\","
		 "\"version\":\"0.6.0\"}");
	LOG_INF("CI addPropertyStatic(DeviceInfo) rc=%d", rc);

	rc = midi2_ci_add_property_dynamic(&g_ci, "ChannelList",
					   channel_list_get, NULL);
	LOG_INF("CI addPropertyDynamic(ChannelList ro) rc=%d", rc);

	rc = midi2_ci_add_property_dynamic(&g_ci, "OverlayRate",
					   overlay_rate_get, overlay_rate_set);
	LOG_INF("CI addPropertyDynamic(OverlayRate rw) rc=%d", rc);

	rc = midi2_ci_pe_set_subscribable(&g_ci, "OverlayRate", true);
	LOG_INF("CI pe_set_subscribable(OverlayRate) rc=%d", rc);
}

/*--------------------------------------------------------------------+
 * Scene timings (extends T-PicoC3 timeline for the K..O additions)
 *--------------------------------------------------------------------*/

#define CH                0

#define kA_StartMs            0
#define kB_Note              60
#define kB_NoteOnMs         400
#define kB_NoteOffMs       6500
#define kB_VibUpdMs          20
#define kB_RegPncMs        1500
#define kB_AsnPncMs        3000
#define kB_MgmtMs          6200
#define kC_BaseNote          72
#define kC_Count              8
#define kC_StartMs         7000
#define kC_StepMs           500
#define kC_EndMs           (kC_StartMs + kC_Count * kC_StepMs)
#define kD_Ms             11500
#define kE_RpnMs          12500
#define kE_NrpnMs         13000
#define kE_RelRpnMs       13500
#define kE_RelNrpnMs      14000
#define kF_Note              64
#define kF_OnMs           14800
#define kF_OffMs          16500
#define kG_Ms             17200
#define kH_Ms             17700
#define kI_Ms             18500
#define kJ_Ms             19500
#define kK_Ms             20500
#define kL_Ms             22500
#define kM_Ms             25000
#define kN_Ms             26500
#define kCycleMs          28500

struct showcase {
	uint32_t cycle_start_ms;
	uint32_t cycle_count;
	bool a_done;
	bool     b_on, b_off, b_reg_pnc, b_asn_pnc, b_mgmt;
	uint32_t b_last_vib;
	uint8_t  c_idx;
	uint32_t c_last_ms;
	bool     c_released;
	bool d_done;
	bool e_rpn_done, e_nrpn_done, e_relrpn_done, e_relnrpn_done;
	bool f_on, f_off;
	bool g_done, h_done, i_done, j_done;
	bool k_done, l_done, m_done, n_done;
};

/* Cheap sine approximation (Bhaskara). -1..+1 over -pi..pi. */
static float sin_approx(float x)
{
	const float PI = 3.14159265f;
	while (x >  PI) { x -= 2.0f * PI; }
	while (x < -PI) { x += 2.0f * PI; }
	float sign = (x < 0.0f) ? -1.0f : 1.0f;
	float ax   = (x < 0.0f) ? -x   :  x;
	float num  = 16.0f * ax * (PI - ax);
	float den  = 5.0f * PI * PI - 4.0f * ax * (PI - ax);
	return sign * (num / den);
}

/*--------------------------------------------------------------------+
 * A — Flex Data Setup (MT 0xD bank 0x00)
 *--------------------------------------------------------------------*/

static void scene_a_flex(struct showcase *s)
{
	if (s->a_done) { return; }
	uint32_t w[4];

	midi2_msg_tempo(w, 0, 50000000u);  /* 120 BPM */
	send_ump(w, 4);
	midi2_msg_time_sig(w, 0, /*num*/ 4, /*denom_exp*/ 2,
			   /*num_32_per_24*/ 8);
	send_ump(w, 4);
	midi2_msg_key_sig(w, 0, /*sharps_flats*/ 0, /*minor*/ false);
	send_ump(w, 4);
	midi2_msg_metronome(w, 0, /*primary*/ 24,
			    /*acc1*/ 0, /*acc2*/ 0, /*acc3*/ 0,
			    /*sub1*/ 0, /*sub2*/ 0);
	send_ump(w, 4);

	/* Chord Name: group-level address (0x01), tonic C (3), Major 7
	 * (chord type 0x03). 21 args including alterations and bass; we
	 * leave all extras zero. */
	midi2_msg_chord_name(w, 0,
			     /*address*/ 1, /*channel*/ 0,
			     /*tonic_sf*/ 0, /*tonic*/ 3, /*chord_type*/ 0x03,
			     0,0, 0,0, 0,0, 0,0,
			     0, 0, 0, 0, 0, 0, 0);
	send_ump(w, 4);

	midi2_msg_stream_start_of_clip(w);
	send_ump(w, 4);

	LOG_INF("[A] Flex Data suite + Start of Clip");
	s->a_done = true;
}

/*--------------------------------------------------------------------+
 * B — Per-Note expression on a sustained C4
 *--------------------------------------------------------------------*/

static void scene_b_per_note(struct showcase *s, uint32_t t, uint32_t now)
{
	uint32_t w[2];

	if (!s->b_on && t >= kB_NoteOnMs) {
		midi2_msg_note_on(w, 0, CH, kB_Note, 0xC000, 0, 0);
		send_ump(w, 2);
		s->b_on = true;
		s->b_last_vib = 0;
		LOG_INF("[B] note on C4 + Per-Note PB vibrato + Per-Note CCs");
	}
	if (s->b_on && !s->b_off && t < kB_NoteOffMs) {
		if (now - s->b_last_vib >= kB_VibUpdMs) {
			float secs = (float)(t - kB_NoteOnMs) / 1000.0f;
			float v = sin_approx(secs * 2.0f * 3.14159265f * 5.0f);
			int32_t off = (int32_t)(v * (float)0x10000000);
			uint32_t pb = (uint32_t)((int64_t)0x80000000 + off);
			midi2_msg_per_note_pb(w, 0, CH, kB_Note, pb);
			send_ump(w, 2);
			s->b_last_vib = now;
		}
	}
	if (!s->b_reg_pnc && s->b_on && t >= kB_RegPncMs) {
		midi2_msg_reg_per_note_ctrl(w, 0, CH, kB_Note,
					    /*idx*/ 7, 0xC0000000u);
		send_ump(w, 2);
		LOG_INF("[B] Registered Per-Note Controller #7 val=0xC0000000");
		s->b_reg_pnc = true;
	}
	if (!s->b_asn_pnc && s->b_on && t >= kB_AsnPncMs) {
		midi2_msg_asn_per_note_ctrl(w, 0, CH, kB_Note,
					    /*idx*/ 74, 0xA0000000u);
		send_ump(w, 2);
		LOG_INF("[B] Assignable Per-Note Controller #74 val=0xA0000000");
		s->b_asn_pnc = true;
	}
	if (!s->b_mgmt && s->b_on && t >= kB_MgmtMs) {
		midi2_msg_per_note_mgmt(w, 0, CH, kB_Note,
					/*detach*/ false, /*reset*/ true);
		send_ump(w, 2);
		LOG_INF("[B] Per-Note Management Reset");
		s->b_mgmt = true;
	}
	if (s->b_on && !s->b_off && t >= kB_NoteOffMs) {
		midi2_msg_per_note_pb(w, 0, CH, kB_Note, 0x80000000u);
		send_ump(w, 2);
		midi2_msg_note_off(w, 0, CH, kB_Note, 0, 0, 0);
		send_ump(w, 2);
		s->b_off = true;
		LOG_INF("[B] note off");
	}
}

/*--------------------------------------------------------------------+
 * C — Resolution showcase, chromatic walk
 *--------------------------------------------------------------------*/

static void scene_c_walk(struct showcase *s, uint32_t t, uint32_t now)
{
	uint32_t w[2];
	if (s->c_idx < kC_Count && t >= kC_StartMs &&
	    (s->c_idx == 0 || (now - s->c_last_ms) >= kC_StepMs)) {
		uint16_t vel = (uint16_t)(0x2000u + (uint32_t)s->c_idx *
				 ((0xFFFFu - 0x2000u) / (kC_Count - 1)));
		if (s->c_idx > 0) {
			midi2_msg_note_off(w, 0, CH,
					   (uint8_t)(kC_BaseNote + s->c_idx - 1),
					   0, 0, 0);
			send_ump(w, 2);
		}
		uint8_t note = (uint8_t)(kC_BaseNote + s->c_idx);
		midi2_msg_note_on(w, 0, CH, note, vel, 0, 0);
		send_ump(w, 2);

		uint32_t cc_val = 0x20000000u + (uint32_t)s->c_idx *
				  ((0xFFFFFFFFu - 0x20000000u) / (kC_Count - 1));
		midi2_msg_cc(w, 0, CH, 74, cc_val);
		send_ump(w, 2);

		uint32_t pb = 0x80000000u + (uint32_t)s->c_idx *
			      ((0xFFFFFFFFu - 0x80000000u) / (kC_Count - 1));
		midi2_msg_pitch_bend(w, 0, CH, pb);
		send_ump(w, 2);

		uint32_t poly = (uint32_t)s->c_idx *
				(0xFFFFFFFFu / (kC_Count - 1));
		midi2_msg_poly_pressure(w, 0, CH, note, poly);
		send_ump(w, 2);

		LOG_INF("[C] step %u note=%u vel=0x%04x cc74=0x%08x pb=0x%08x poly=0x%08x",
			s->c_idx, note, vel, cc_val, pb, poly);
		s->c_last_ms = now;
		s->c_idx++;
	}
	if (!s->c_released && s->c_idx == kC_Count && t >= kC_EndMs) {
		midi2_msg_note_off(w, 0, CH,
				   (uint8_t)(kC_BaseNote + kC_Count - 1),
				   0, 0, 0);
		send_ump(w, 2);
		midi2_msg_pitch_bend(w, 0, CH, 0x80000000u);
		send_ump(w, 2);
		s->c_released = true;
		LOG_INF("[C] walk end (PB reset)");
	}
}

/*--------------------------------------------------------------------+
 * D — Program Change with Bank
 *--------------------------------------------------------------------*/

static void scene_d_program(struct showcase *s, uint32_t t)
{
	if (s->d_done || t < kD_Ms) { return; }
	uint32_t w[2];
	midi2_msg_program(w, 0, CH, /*program*/ 42,
			  /*bank_msb*/ 0x10, /*bank_lsb*/ 0x05,
			  /*bank_valid*/ true);
	send_ump(w, 2);
	LOG_INF("[D] Program=42 with Bank MSB=0x10 LSB=0x05 (single UMP)");
	s->d_done = true;
}

/*--------------------------------------------------------------------+
 * E — RPN / NRPN / Relative variants
 *--------------------------------------------------------------------*/

static void scene_e_rpn_nrpn(struct showcase *s, uint32_t t)
{
	uint32_t w[2];
	if (!s->e_rpn_done && t >= kE_RpnMs) {
		midi2_msg_rpn(w, 0, CH, /*msb*/ 0, /*lsb*/ 0, 0x40000000u);
		send_ump(w, 2);
		LOG_INF("[E] RPN 0/0 val=0x40000000");
		s->e_rpn_done = true;
	}
	if (!s->e_nrpn_done && t >= kE_NrpnMs) {
		midi2_msg_nrpn(w, 0, CH, 0x12, 0x34, 0xDEADBEEFu);
		send_ump(w, 2);
		LOG_INF("[E] NRPN 0x12/0x34 val=0xDEADBEEF");
		s->e_nrpn_done = true;
	}
	if (!s->e_relrpn_done && t >= kE_RelRpnMs) {
		midi2_msg_rel_rpn(w, 0, CH, 0, 0, 0x01000000u);
		send_ump(w, 2);
		LOG_INF("[E] Relative RPN 0/0 delta=+0x01000000");
		s->e_relrpn_done = true;
	}
	if (!s->e_relnrpn_done && t >= kE_RelNrpnMs) {
		midi2_msg_rel_nrpn(w, 0, CH, 0x12, 0x34, 0xFF800000u);
		send_ump(w, 2);
		LOG_INF("[E] Relative NRPN 0x12/0x34 delta=-0x00800000");
		s->e_relnrpn_done = true;
	}
}

/*--------------------------------------------------------------------+
 * F — Note Attribute pitch_7_9 (microtonal)
 *--------------------------------------------------------------------*/

static void scene_f_attribute(struct showcase *s, uint32_t t)
{
	uint32_t w[2];
	if (!s->f_on && t >= kF_OnMs) {
		uint16_t attr = (uint16_t)(((uint16_t)kF_Note << 9) | 256);
		midi2_msg_note_on(w, 0, CH, kF_Note, 0xC000,
				  /*attr_type*/ 0x03, /*attr_data*/ attr);
		send_ump(w, 2);
		LOG_INF("[F] Note On Attribute pitch_7_9 E4+50c attr=0x%04x",
			attr);
		s->f_on = true;
	}
	if (!s->f_off && t >= kF_OffMs) {
		midi2_msg_note_off(w, 0, CH, kF_Note, 0,
				   /*attr_type*/ 0x03, /*attr_data*/ 0);
		send_ump(w, 2);
		s->f_off = true;
	}
}

/*--------------------------------------------------------------------+
 * G — SysEx7 Universal Identity Reply (fragmented)
 *--------------------------------------------------------------------*/

static void scene_g_sysex7(struct showcase *s, uint32_t t)
{
	if (s->g_done || t < kG_Ms) { return; }

	uint32_t w[2];

	/* G.1 - Universal Identity Reply (12 bytes, Start + End). */
	static const uint8_t identity[] = {
		0x7E, 0x7F, 0x06, 0x02,
		0x7D, 0x01, 0x00, 0x40,
		0x00, 0x05, 0x00, 0x00,
	};
	midi2_msg_sysex7_packet(w, 0, /*Start*/ 0x10, identity,     6);
	send_ump(w, 2);
	midi2_msg_sysex7_packet(w, 0, /*End*/   0x30, identity + 6, 6);
	send_ump(w, 2);
	LOG_INF("[G.1] SysEx7 Identity Reply (12 bytes, Start+End)");

	/* G.2 - 18-byte bulk MFR-specific (private Roland-style envelope:
	 * MFR 7D 00 00 + device-id + 14 payload bytes), forced into three
	 * packets to exercise the Continue status (0x20). */
	static const uint8_t bulk[18] = {
		0x7D, 0x00, 0x00, 0x42, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
	};
	midi2_msg_sysex7_packet(w, 0, /*Start*/    0x10, bulk,      6);
	send_ump(w, 2);
	midi2_msg_sysex7_packet(w, 0, /*Continue*/ 0x20, bulk +  6, 6);
	send_ump(w, 2);
	midi2_msg_sysex7_packet(w, 0, /*End*/      0x30, bulk + 12, 6);
	send_ump(w, 2);
	LOG_INF("[G.2] SysEx7 bulk 18 bytes (Start+Continue+End)");

	s->g_done = true;
}

/*--------------------------------------------------------------------+
 * H — Delta Clockstamp
 *--------------------------------------------------------------------*/

static void scene_h_dctpq(struct showcase *s, uint32_t t)
{
	if (s->h_done || t < kH_Ms) { return; }
	uint32_t w;
	w = midi2_msg_dctpq(480);
	send_ump(&w, 1);
	w = midi2_msg_delta_clockstamp(240);
	send_ump(&w, 1);
	LOG_INF("[H] DCTPQ=480 + Delta Clockstamp=240 ticks");
	s->h_done = true;
}

/*--------------------------------------------------------------------+
 * I — PE Notify
 *--------------------------------------------------------------------*/

static void scene_i_pe_notify(struct showcase *s, uint32_t t)
{
	if (s->i_done || t < kI_Ms) { return; }
	(void)snprintk(g_overlay_rate, sizeof(g_overlay_rate),
		       "{\"rateHz\":%u}", 50u + s->cycle_count);
	(void)midi2_ci_notify_property_changed(&g_ci, "OverlayRate");
	LOG_INF("[I] PE Notify OverlayRate=%s subscribers=%u",
		g_overlay_rate,
		(unsigned)midi2_ci_get_subscriber_count(&g_ci));
	s->i_done = true;
}

/*--------------------------------------------------------------------+
 * J — End of Clip
 *--------------------------------------------------------------------*/

static void scene_j_clip_end(struct showcase *s, uint32_t t)
{
	if (s->j_done || t < kJ_Ms) { return; }
	uint32_t w[4];
	midi2_msg_stream_end_of_clip(w);
	send_ump(w, 4);
	LOG_INF("[J] End of Clip");
	s->j_done = true;
}

/*--------------------------------------------------------------------+
 * K — MIDI 1.0 Channel Voice burst (MT 0x2)
 *--------------------------------------------------------------------*/

static void scene_k_midi1_cv(struct showcase *s, uint32_t t)
{
	if (s->k_done || t < kK_Ms) { return; }

	uint32_t w;
	w = midi2_msg_from_midi1(0, 0x90, /*note*/ 60, /*vel*/ 0x40);
	send_ump(&w, 1);
	w = midi2_msg_from_midi1(0, 0x80, 60, 0x00);
	send_ump(&w, 1);
	w = midi2_msg_from_midi1(0, 0xB0, /*cc*/ 7, /*vol*/ 0x60);
	send_ump(&w, 1);
	w = midi2_msg_from_midi1(0, 0xE0, /*pb_lsb*/ 0x00, /*pb_msb*/ 0x40);
	send_ump(&w, 1);
	w = midi2_msg_from_midi1(0, 0xC0, /*program*/ 5, 0x00);
	send_ump(&w, 1);

	LOG_INF("[K] MIDI 1.0 CV burst: NoteOn/Off, CC7, PitchBend, Program");
	s->k_done = true;
}

/*--------------------------------------------------------------------+
 * L — System Common / Real-Time burst (MT 0x1)
 *--------------------------------------------------------------------*/

static void scene_l_system(struct showcase *s, uint32_t t)
{
	if (s->l_done || t < kL_Ms) { return; }

	uint32_t w;
	w = midi2_msg_system_timing_clock(0);          send_ump(&w, 1);
	w = midi2_msg_system_start(0);                 send_ump(&w, 1);
	w = midi2_msg_system_continue(0);              send_ump(&w, 1);
	w = midi2_msg_system_stop(0);                  send_ump(&w, 1);
	w = midi2_msg_system_active_sensing(0);        send_ump(&w, 1);
	w = midi2_msg_system_reset(0);                 send_ump(&w, 1);
	w = midi2_msg_system_tune_request(0);          send_ump(&w, 1);
	w = midi2_msg_system_mtc(0, /*quarter*/ 0x42); send_ump(&w, 1);
	w = midi2_msg_system_song_select(0, 5);        send_ump(&w, 1);
	w = midi2_msg_system_song_position(0, 1024);   send_ump(&w, 1);

	LOG_INF("[L] System burst: 10 messages (Real-Time + Common)");
	s->l_done = true;
}

/*--------------------------------------------------------------------+
 * M — Flex Data Metadata Text (project name + composer)
 *--------------------------------------------------------------------*/

static void scene_m_metadata(struct showcase *s, uint32_t t)
{
	if (s->m_done || t < kM_Ms) { return; }

	uint32_t w[4];
	const char *project = "midi2-showcase";   /* 14 bytes, single UMP */
	const char *composer = "Saulo Verissimo"; /* 15 bytes, single UMP */

	midi2_msg_flex_text(w, 0,
			    /*format*/ 0,           /* complete */
			    /*address*/ 1,          /* group */
			    /*channel*/ 0,
			    MIDI2_FLEX_BANK_METADATA,
			    MIDI2_FLEX_TEXT_PROJECT_NAME,
			    (const uint8_t *)project, 14);
	send_ump(w, 4);

	midi2_msg_flex_text(w, 0,
			    /*format*/ 0,
			    /*address*/ 1,
			    /*channel*/ 0,
			    MIDI2_FLEX_BANK_METADATA,
			    MIDI2_FLEX_TEXT_COMPOSER_NAME,
			    (const uint8_t *)composer, 15);
	send_ump(w, 4);

	LOG_INF("[M] Flex Metadata Text: ProjectName='%s' ComposerName='%s'",
		project, composer);
	s->m_done = true;
}

/*--------------------------------------------------------------------+
 * N — Utility additions: Noop + JR Clock (distinct from heartbeat)
 *--------------------------------------------------------------------*/

static void scene_n_utility(struct showcase *s, uint32_t t)
{
	if (s->n_done || t < kN_Ms) { return; }

	uint32_t w;
	w = midi2_msg_noop();              send_ump(&w, 1);
	w = midi2_msg_jr_clock(0x0001);    send_ump(&w, 1);

	LOG_INF("[N] Utility: Noop + JR Clock");
	s->n_done = true;
}

/*--------------------------------------------------------------------+
 * Top-level cycle driver
 *--------------------------------------------------------------------*/

static void showcase_step(struct showcase *s)
{
	if (!atomic_get(&g_midi_ready)) { return; }

	uint32_t now = k_uptime_get_32();

	if (s->cycle_start_ms == 0 || (now - s->cycle_start_ms) >= kCycleMs) {
		uint32_t prev = s->cycle_count;
		memset(s, 0, sizeof(*s));
		s->cycle_start_ms = now;
		s->cycle_count    = prev + 1;
		LOG_INF("--- cycle %u start at %u ms ---",
			s->cycle_count, now);
	}
	uint32_t t = now - s->cycle_start_ms;

	scene_a_flex(s);
	scene_b_per_note(s, t, now);
	scene_c_walk(s, t, now);
	scene_d_program(s, t);
	scene_e_rpn_nrpn(s, t);
	scene_f_attribute(s, t);
	scene_g_sysex7(s, t);
	scene_h_dctpq(s, t);
	scene_i_pe_notify(s, t);
	scene_j_clip_end(s, t);
	scene_k_midi1_cv(s, t);
	scene_l_system(s, t);
	scene_m_metadata(s, t);
	scene_n_utility(s, t);
}

/*--------------------------------------------------------------------+
 * Main
 *--------------------------------------------------------------------*/

int main(void)
{
	int err;

	if (!device_is_ready(midi_dev)) {
		LOG_ERR("MIDI device not ready");
		return -1;
	}

	if (led.port) {
		err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
		if (err) {
			LOG_WRN("LED setup failed (%d), continuing without",
				err);
		}
	}

	midi2_dispatch_init(&g_dp);
	g_dp.on_note_on = on_note_on_cb;
	g_dp.on_cc      = on_cc_cb;
	g_dp.on_program = on_program_cb;

	midi2_proc_init(&g_proc,
			g_sysex7_buf, sizeof(g_sysex7_buf),
			NULL, 0);  /* SysEx8 not in market today; inbound ignored */
	g_proc.on_ump      = midi2_dispatch_feed;
	g_proc.context     = &g_dp;
	g_proc.on_sysex7   = on_sysex7_complete;

	install_ci_bootstrap();

	usbd_midi_set_ops(midi_dev, &midi_ops);

	err = midi2_usbd_setup();
	if (err) {
		LOG_ERR("USB device init failed (%d)", err);
		return -1;
	}
	LOG_INF("USB device enabled, waiting for host enumeration");

	k_timer_start(&jr_heartbeat_timer, K_MSEC(500), K_MSEC(500));

	struct showcase s = {0};
	while (true) {
		showcase_step(&s);
		k_msleep(10);
	}

	return 0;
}
