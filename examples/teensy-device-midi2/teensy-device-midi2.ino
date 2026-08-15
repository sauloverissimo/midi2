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

/* Declared once, by byte, and reused by every path that carries the
 * identity: the UMP Stream responder, the boot announcement, MIDI-CI
 * Discovery and the Property Exchange DeviceInfo JSON below. The packed
 * words and the JSON fragments derive from the same bytes at compile
 * time, so a change here reaches every channel or none. */
#define DEV_MFR_B1      125   /* 0x7D educational SysEx prefix */
#define DEV_MFR_B2      0
#define DEV_MFR_B3      0
#define DEV_FAMILY_LSB  1     /* 14-bit values as 7-bit LSB/MSB pairs */
#define DEV_FAMILY_MSB  0
#define DEV_MODEL_LSB   1
#define DEV_MODEL_MSB   0
#define DEV_VER_B1      0     /* four Software Revision bytes */
#define DEV_VER_B2      0
#define DEV_VER_B3      10
#define DEV_VER_B4      0
#define DEV_STR_(x) #x
#define DEV_STR(x)  DEV_STR_(x)

static const uint32_t kMfr       = ((uint32_t)DEV_MFR_B1 << 16) | ((uint32_t)DEV_MFR_B2 << 8) | DEV_MFR_B3;
static const uint16_t kFamily    = ((uint16_t)DEV_FAMILY_MSB << 7) | DEV_FAMILY_LSB;
static const uint16_t kModel     = ((uint16_t)DEV_MODEL_MSB << 7) | DEV_MODEL_LSB;
static const uint32_t kVersion   = ((uint32_t)DEV_VER_B1 << 24) | ((uint32_t)DEV_VER_B2 << 16)
                                 | ((uint32_t)DEV_VER_B3 << 8)  | DEV_VER_B4;
static const char     kEpName[]  = "Teensy 4.1 MIDI 2.0";
static const char     kProdId[]  = "midi2.diy-teensy-0001";
static const char     kFbName[]  = "Main";
static const uint8_t  kFbNum     = 0;

static const char kDeviceInfo[] =
    "{\"manufacturerId\":[" DEV_STR(DEV_MFR_B1) "," DEV_STR(DEV_MFR_B2) "," DEV_STR(DEV_MFR_B3) "],"
    "\"familyId\":[" DEV_STR(DEV_FAMILY_LSB) "," DEV_STR(DEV_FAMILY_MSB) "],"
    "\"modelId\":[" DEV_STR(DEV_MODEL_LSB) "," DEV_STR(DEV_MODEL_MSB) "],"
    "\"versionId\":[" DEV_STR(DEV_VER_B1) "," DEV_STR(DEV_VER_B2) "," DEV_STR(DEV_VER_B3) "," DEV_STR(DEV_VER_B4) "],"
    "\"manufacturer\":\"midi2.diy\","
    "\"family\":\"Teensy 4.x\",\"model\":\"Teensy 4.1 MIDI 2.0\","
    "\"version\":\"0.10.0\"}";
static const char kChannelList[] = "[{\"title\":\"Main\",\"channel\":1}]";
static const char kProgramList[] = "[{\"title\":\"Default\",\"bankPC\":[0,0,0]}]";

/* Caller-owned storage: the core allocates nothing. */
static midi2_ci_state    ci;
static uint8_t           ci_profiles[2][5];
static midi2_ci_property ci_props[4];

static midi2_proc_state  proc;
static uint8_t           sysex7_buf[256];
static midi2_dispatch    dp;

/* MIDI-CI replies leave over the UMP endpoint, one packet per call. The
 * core stops a multi-packet reply at the first short write, so the actual
 * word count goes back instead of an assumed success. */
static uint32_t ci_write(const uint32_t *words, uint32_t count, void *ctx) {
    (void)ctx;
    return (uint32_t)usbMIDI2.write(words, (uint8_t)count);
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
static void send_endpoint_info(void) {
    uint32_t w[4];
    midi2_msg_stream_endpoint_info(w, /*ump major*/ 1, /*ump minor*/ 1,
                                   /*static_fb*/ true, /*num_fb*/ 1,
                                   /*midi2*/ true, /*midi1*/ true,
                                   /*rx_jr*/ false, /*tx_jr*/ false);
    usbMIDI2.write(w, 4);
}

/* SysEx id in the first of the three manufacturer bytes: {0x7D,0x00,0x00} */
static void send_device_identity(void) {
    uint32_t w[4];
    midi2_msg_stream_device_identity(w, kMfr, kFamily, kModel, kVersion);
    usbMIDI2.write(w, 4);
}

/* This recipe emits Note On/Off and MIDI-CI only, so max_sysex8 is 0. */
static void send_fb_info(void) {
    uint32_t w[4];
    midi2_msg_stream_fb_info(w, /*active*/ true, kFbNum,
                             /*direction*/ 3 /*bidirectional*/,
                             /*ui_hint*/ 3 /*bidirectional*/,
                             /*first_group*/ 0, /*num_groups*/ 1,
                             /*midi_ci_ver*/ 2, /*max_sysex8*/ 0);
    usbMIDI2.write(w, 4);
}

static void send_config_notify(uint8_t protocol) {
    uint32_t w[4];
    midi2_msg_stream_config_notify(w, protocol, false, false);
    usbMIDI2.write(w, 4);
}

/* The Teensy core is transport only, so the sketch answers Discovery itself.
 * Each filter bit asks for one reply (M2-104-UM 7.1.1). */
static void on_ep_discovery(uint8_t ver_major, uint8_t ver_minor,
                            uint8_t filter, void *ctx) {
    (void)ver_major; (void)ver_minor; (void)ctx;
    if (filter & 0x01) send_endpoint_info();
    if (filter & 0x02) send_device_identity();
    if (filter & 0x04) midi2_proc_send_endpoint_name(kEpName, ci_write, NULL);
    if (filter & 0x08) midi2_proc_send_product_id(kProdId, ci_write, NULL);
    if (filter & 0x10) send_config_notify(0x02);
}

static void on_fb_discovery(uint8_t fb_num, uint8_t filter, void *ctx) {
    (void)ctx;
    if (fb_num != kFbNum && fb_num != 0xFF) return;
    if (filter & 0x01) send_fb_info();
    if (filter & 0x02) midi2_proc_send_fb_name(kFbNum, kFbName, ci_write, NULL);
}

static void on_config_request(uint8_t protocol, bool rx_jr, bool tx_jr,
                              void *ctx) {
    (void)rx_jr; (void)tx_jr; (void)ctx;
    if (protocol != 0x01 && protocol != 0x02) protocol = 0x02;
    send_config_notify(protocol);
}

/* Stream messages drive the responder; anything else is left alone. */
static void on_ump(const uint32_t *words, uint8_t word_count, void *ctx) {
    (void)ctx;
    if (midi2_msg_get_mt(words) == MIDI2_MT_STREAM)
        midi2_dispatch_feed(words, word_count, &dp);
}

/* Announce the identity once at boot for a host that captured enumeration
 * before the sketch was up. A host running active Endpoint Discovery
 * (Windows MIDI Services) is served by the responder above. */
static void announce_stream_identity(void) {
    send_endpoint_info();
    send_device_identity();
    send_fb_info();
    midi2_proc_send_endpoint_name(kEpName, ci_write, NULL);
    midi2_proc_send_fb_name(kFbNum, kFbName, ci_write, NULL);
    midi2_proc_send_product_id(kProdId, ci_write, NULL);
}

void setup() {
    usbMIDI2.begin();

    midi2_ci_init(&ci, seed_from_hardware(),
                  ci_profiles, 2, ci_props, 4);
    midi2_ci_set_write_fn(&ci, ci_write, NULL);
    midi2_ci_set_identity(&ci, kMfr, kFamily, kModel, kVersion);
    midi2_ci_set_nak_on_unknown(&ci, true);

    midi2_ci_add_profile(&ci, kProfile);
    midi2_ci_add_property_static(&ci, "DeviceInfo",  kDeviceInfo);
    midi2_ci_add_property_static(&ci, "ChannelList", kChannelList);
    midi2_ci_add_property_static(&ci, "ProgramList", kProgramList);

    midi2_dispatch_init(&dp);
    dp.on_endpoint_discovery = on_ep_discovery;
    dp.on_fb_discovery       = on_fb_discovery;
    dp.on_config_request     = on_config_request;

    midi2_proc_init(&proc, sysex7_buf, sizeof(sysex7_buf), NULL, 0);
    proc.on_sysex7 = on_sysex7;
    proc.on_ump    = on_ump;

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
