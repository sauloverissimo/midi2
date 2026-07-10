/*
 * midi2 | Teensy 4.x USB MIDI 2.0 device
 *
 * A complete USB MIDI 2.0 device built on the portable midi2 C99 core and
 * the Teensy core's native UMP endpoint (usbMIDI2). It answers MIDI-CI
 * Discovery, Profile Configuration and Property Exchange, and emits a short
 * MIDI 2.0 demo so a host sees live traffic. Validated end to end against the
 * official MIDI 2.0 Workbench.
 *
 * Board setup: Tools > USB Type > "MIDI" on a Teensy 4.x, using the Teensy
 * core build that ships the USB MIDI 2.0 (usbMIDI2) endpoint.
 */

#include <midi2.h>
#include "usb_midi2.h"  /* Teensy core UMP endpoint (usbMIDI2), USB Type "MIDI 2.0" */

/* Device identity. 0x7D is the non-commercial manufacturer prefix; a shipping
 * product would use a licensed Manufacturer ID. The MIDI-CI Discovery bytes
 * and the Property Exchange DeviceInfo below carry the same values. */
static const uint8_t  kProfile[5] = {0x7E, 0x00, 0x00, 0x01, 0x00};

static const char kDeviceInfo[] =
    "{\"manufacturerId\":[125,0,0],\"familyId\":[1,0],\"modelId\":[1,0],"
    "\"versionId\":[0,0,7,0],\"manufacturer\":\"midi2.diy\","
    "\"family\":\"Teensy 4.x\",\"model\":\"Teensy 4.1 MIDI 2.0\","
    "\"version\":\"0.7.0\"}";
static const char kChannelList[] = "[{\"title\":\"Main\",\"channel\":1}]";
static const char kProgramList[] = "[{\"title\":\"Default\",\"bankPC\":[0,0,0]}]";

/* Caller-owned storage: the core allocates nothing. */
static midi2_ci_state    ci;
static uint8_t           ci_profiles[2][5];
static midi2_ci_property ci_props[4];

static midi2_proc_state  proc;
static uint8_t           sysex7_buf[256];

/* MIDI-CI replies leave over the UMP endpoint, one packet per call. */
static uint32_t ci_write(const uint32_t *words, uint32_t count, void *ctx) {
    (void)ctx;
    usbMIDI2.write(words, (uint8_t)count);
    return count;
}

/* midi2_proc reassembles inbound SysEx7 and hands the complete message here. */
static void on_sysex7(uint8_t group, const uint8_t *data, uint16_t len,
                      void *ctx) {
    (void)ctx;
    midi2_ci_process_sysex(&ci, group, data, len);
}

static uint32_t seed_from_hardware(void) {
    uint32_t s = micros();
    for (int i = 0; i < 8; ++i) {
        s = (s << 3) ^ analogRead(A0) ^ ARM_DWT_CYCCNT;
    }
    return s;
}

/* Announce the UMP Stream identity (Endpoint Info, Device Identity, Endpoint
 * Name, Product Instance Id, Function Block Info). The Teensy core leaves UMP
 * Stream Discovery to the application by design. A host that reads the static
 * USB descriptors (Linux snd-ump) enumerates without this; a host that runs
 * active UMP Endpoint Discovery (Windows MIDI Services) needs it to register a
 * complete endpoint. */
static void announce_stream_identity(void) {
    uint32_t w[4];

    midi2_msg_stream_endpoint_info(w, /*ump major*/ 1, /*ump minor*/ 1,
                                   /*static_fb*/ true, /*num_fb*/ 1,
                                   /*midi2*/ true, /*midi1*/ true,
                                   /*rx_jr*/ false, /*tx_jr*/ false);
    usbMIDI2.write(w, 4);

    midi2_msg_stream_device_identity(w, /*manufacturer*/ 0x7D,
                                     /*family*/ 0x0001, /*model*/ 0x0001,
                                     /*version*/ 0x00070000);
    usbMIDI2.write(w, 4);

    midi2_msg_stream_fb_info(w, /*active*/ true, /*fb_num*/ 0,
                             /*direction*/ 3 /*bidirectional*/,
                             /*ui_hint*/ 3 /*bidirectional*/,
                             /*first_group*/ 0, /*num_groups*/ 1,
                             /*midi_ci_ver*/ 2, /*max_sysex8*/ 0,
                             /*protocol*/ 2 /*MIDI 2.0*/);
    usbMIDI2.write(w, 4);

    midi2_proc_send_endpoint_name("Teensy 4.1 MIDI 2.0", ci_write, NULL);
    midi2_proc_send_fb_name(0, "Main", ci_write, NULL);
    midi2_proc_send_product_id("midi2.diy-teensy-0001", ci_write, NULL);
}

void setup() {
    usbMIDI2.begin();

    midi2_ci_init(&ci, seed_from_hardware(),
                  ci_profiles, 2, ci_props, 4);
    midi2_ci_set_write_fn(&ci, ci_write, NULL);
    midi2_ci_set_identity(&ci, /*manufacturer*/ 0x7D, /*family*/ 0x0001,
                          /*model*/ 0x0001, /*sw_rev*/ 0x00070000);
    midi2_ci_set_nak_on_unknown(&ci, true);

    midi2_ci_add_profile(&ci, kProfile);
    midi2_ci_add_property_static(&ci, "DeviceInfo",  kDeviceInfo);
    midi2_ci_add_property_static(&ci, "ChannelList", kChannelList);
    midi2_ci_add_property_static(&ci, "ProgramList", kProgramList);

    midi2_proc_init(&proc, sysex7_buf, sizeof(sysex7_buf), NULL, 0);
    proc.on_sysex7 = on_sysex7;

    announce_stream_identity();
}

/* A one-note MIDI 2.0 demo, 16-bit velocity, once per second. */
static void demo_tick(void) {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last < 1000) {
        return;
    }
    last = now;

    uint32_t w[2];
    midi2_msg_note_on(w, /*group*/ 0, /*channel*/ 0, /*note*/ 60,
                      /*velocity16*/ 0xC000, /*attr_type*/ 0, /*attr_data*/ 0);
    usbMIDI2.write(w, 2);
    midi2_msg_note_off(w, 0, 0, 60, 0x0000, 0, 0);
    usbMIDI2.write(w, 2);
}

void loop() {
    uint32_t words[4];
    uint8_t  count;
    while (usbMIDI2.read(words, &count)) {
        midi2_proc_feed(&proc, words, count);
    }
    demo_tick();
}
