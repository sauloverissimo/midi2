/*
 * midi2 basic-usage example, Arduino sketch.
 *
 * Demonstrates the four core surfaces of the library on any
 * Arduino-compatible board:
 *   1. Build a UMP message word-by-word with midi2_msg_*
 *   2. Scale 7-bit and 16-bit values bidirectionally
 *   3. Dispatch a UMP through midi2_dispatch into typed callbacks
 *   4. Drive a midi2_proc state for SysEx reassembly + group filtering
 *
 * The sketch does not depend on any specific MIDI transport; it builds
 * messages, parses them, and prints the result over Serial. Plug into
 * a MIDI 2.0 transport (TinyUSB fork, Teensy core fork, ESP32 native,
 * BLE, ESP-NOW) by wiring the dispatch context's write functions.
 *
 * Tested on Teensy 4.x (architectures=teensy4) and on host-side g++
 * via the same source tree.
 */

#include <midi2.h>

// Latest tempo received via dispatch, in BPM. The dispatch callback
// updates this and the loop prints it once per second.
static volatile uint32_t g_last_tempo_bpm = 0;

static void on_note_on(uint8_t group, uint8_t channel, uint8_t note,
                       uint16_t velocity, uint8_t attr_type,
                       uint16_t attr_data, void *ctx) {
    (void)group; (void)attr_type; (void)attr_data; (void)ctx;
    Serial.print("Note On  ch=");
    Serial.print(channel);
    Serial.print(" note=");
    Serial.print(note);
    Serial.print(" vel16=0x");
    Serial.println(velocity, HEX);
}

static void on_cc(uint8_t group, uint8_t channel, uint8_t index,
                  uint32_t value, void *ctx) {
    (void)group; (void)ctx;
    Serial.print("CC       ch=");
    Serial.print(channel);
    Serial.print(" idx=");
    Serial.print(index);
    Serial.print(" val32=0x");
    Serial.println(value, HEX);
}

static void on_tempo(uint8_t group, uint32_t ten_ns_per_qn, void *ctx) {
    (void)group; (void)ctx;
    g_last_tempo_bpm = (uint32_t)(60000000000.0 / (double)ten_ns_per_qn / 10.0);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        // Wait for USB CDC on boards where Serial is over USB.
    }

    Serial.println("=== midi2 basic-usage ===");

    // -------- 1. Build messages --------

    uint32_t w[4];

    midi2_msg_note_on(w, /*group*/ 0, /*channel*/ 0,
                      /*note*/ 60, /*velocity16*/ 0xC000,
                      /*attr_type*/ 0, /*attr_data*/ 0);
    Serial.print("Note On C4 vel=0xC000 -> 0x");
    Serial.print(w[0], HEX);
    Serial.print(" 0x");
    Serial.println(w[1], HEX);

    midi2_msg_cc(w, /*group*/ 0, /*channel*/ 0,
                 /*index*/ 74, /*value32*/ 0x80000000);
    Serial.print("CC #74 50%      -> 0x");
    Serial.print(w[0], HEX);
    Serial.print(" 0x");
    Serial.println(w[1], HEX);

    // -------- 2. Value scaling --------

    Serial.print("7->16 bit: 127 -> 0x");
    Serial.println(midi2_msg_scale_up_7to16(127), HEX);

    Serial.print("16->7 bit: 0xC000 -> ");
    Serial.println(midi2_msg_scale_down_16to7(0xC000));

    // -------- 3. Typed dispatch --------

    midi2_dispatch dp;
    midi2_dispatch_init(&dp);
    dp.on_note_on = on_note_on;
    dp.on_cc      = on_cc;
    dp.on_tempo   = on_tempo;

    // Build and feed messages back into the dispatch to trigger callbacks.
    midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0, 0);
    midi2_dispatch_feed(w, 2, &dp);

    midi2_msg_cc(w, 0, 0, 1, 0x40000000);
    midi2_dispatch_feed(w, 2, &dp);

    midi2_msg_tempo(w, 0, /*ten_ns_per_qn*/ 50000000u);  // 120 BPM
    midi2_dispatch_feed(w, 4, &dp);

    // -------- 4. Proc state for SysEx reassembly --------

    static uint8_t sysex7_buf[128];
    static uint8_t sysex8_buf[256];
    static midi2_proc_state proc;
    midi2_proc_init(&proc, sysex7_buf, sizeof(sysex7_buf),
                          sysex8_buf, sizeof(sysex8_buf));

    proc.on_ump   = midi2_dispatch_feed;
    proc.context  = &dp;

    // Same NoteOn fed through proc (which would also reassemble SysEx).
    midi2_msg_note_on(w, 0, 5, 36, 0x8000, 0, 0);
    midi2_proc_feed(&proc, w, 2);

    Serial.println("Setup complete. Loop will print received tempo.");
}

void loop() {
    static uint32_t last_print = 0;
    uint32_t now = millis();
    if (now - last_print < 1000) return;
    last_print = now;

    Serial.print("g_last_tempo_bpm = ");
    Serial.println(g_last_tempo_bpm);
}
