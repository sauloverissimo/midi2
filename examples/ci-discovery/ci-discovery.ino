/*
 * midi2 ci-discovery example, Arduino sketch.
 *
 * Demonstrates the MIDI-CI Discovery responder running standalone:
 *   1. Initialise a midi2_ci_state with profiles + properties storage.
 *   2. Register one custom Profile.
 *   3. Feed an incoming Discovery Inquiry (Universal SysEx).
 *   4. Watch the auto-generated Discovery Reply hit the platform write
 *      callback, which here just dumps the raw bytes over Serial.
 *
 * No USB MIDI transport required. Plug into TinyUSB / Teensy native
 * USB / BLE-MIDI by replacing platform_write with a real send call.
 */

#include <midi2.h>

static uint32_t platform_write(const uint32_t *words, uint32_t count, void *ctx) {
    (void)ctx;
    for (uint32_t i = 0; i < count; ++i) {
        Serial.print(" 0x");
        Serial.print(words[i], HEX);
    }
    Serial.println();
    return count;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    Serial.println("=== midi2 ci-discovery ===");

    static uint8_t profiles[2][5];
    static midi2_ci_property props[1];
    static midi2_ci_state ci;

    midi2_ci_init(&ci, /*muid*/ 0xDEADBEEF, profiles, 2, props, 1);
    midi2_ci_set_identity(&ci,
                          /*manufacturer*/ 0x00AABB,
                          /*family*/       0x0001,
                          /*model*/        0x0001,
                          /*sw_rev*/       0x00010000);
    midi2_ci_set_write_fn(&ci, platform_write, NULL);

    const uint8_t profile_id[5] = {0x00, 0x21, 0x09, 0x00, 0x01};
    midi2_ci_add_profile(&ci, profile_id);

    Serial.println("Identity + 1 custom profile registered. Sending fake Discovery Inquiry.");

    // Universal SysEx Discovery Inquiry header. midi2_ci_process_sysex
    // consumes it and the responder pushes a Discovery Reply through
    // platform_write.
    const uint8_t discovery_req[] = {
        0x7E, 0x7F, 0x0D, 0x70,  // Universal SysEx + CI sub-id
        0x01, 0x00, 0x00, 0x00,  // source MUID
        0x7F, 0x7F, 0x7F, 0x7F,  // dest MUID = broadcast
        0x00, 0x00               // CI version + capabilities
    };

    Serial.print("Reply UMP words:");
    if (midi2_ci_process_sysex(&ci, /*group*/ 0, discovery_req, sizeof(discovery_req))) {
        Serial.println("Auto-reply fired.");
    } else {
        Serial.println("(no reply, check inputs)");
    }
}

void loop() {
    // Idle. A real recipe would poll its USB / BLE transport and feed
    // incoming UMP into midi2_ci_process_sysex / midi2_dispatch_feed.
    delay(1000);
}
