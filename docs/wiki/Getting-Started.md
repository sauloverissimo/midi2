# Getting Started

## Requirements

- A C99 compiler (gcc, clang, MSVC, arm-none-eabi-gcc)
- `make` (optional, for running tests)

No external dependencies. Only `stdint.h`, `stdbool.h`, and `string.h` from the C standard library.

## Building and Testing

```bash
git clone https://github.com/sauloverissimo/midi2.git
cd midi2
make test
```

This compiles and runs all 252 tests with `-Wall -Wextra -Wpedantic`. You can also specify a compiler or enable sanitizers:

```bash
make CC=clang test                                              # clang
make CC="gcc -fsanitize=address,undefined" test                 # memory + UB checks
make CC="gcc -m32" test                                         # 32-bit verification
```

## First Example: Build and Parse a Note On

```c
#include "midi2.h"

int main(void) {
    /* Build a MIDI 2.0 Note On: group 0, channel 0, note 60 (C4), velocity 75% */
    uint32_t w[2];
    midi2_msg_note_on(w, 0, 0, 60, 0xC000, 0);

    /* Parse it back */
    uint8_t mt      = midi2_msg_get_mt(w);        /* 0x04 = MIDI 2.0 CV */
    uint8_t group   = midi2_msg_get_group(w);     /* 0 */
    uint8_t channel = midi2_msg_get_channel(w);   /* 0 */
    uint8_t note    = midi2_msg_get_note(w);      /* 60 */
    uint16_t vel    = midi2_msg_get_velocity(w);  /* 0xC000 */

    /* Scale a legacy 7-bit velocity to 16-bit */
    uint16_t vel16 = midi2_msg_scale_up_7to16(100);  /* bit-replicated */
    uint8_t vel7   = midi2_msg_scale_down_16to7(vel16);  /* round-trip safe */

    return 0;
}
```

Compile with: `gcc -std=c99 -I src example.c -o example` (midi2_msg is header-only, no `.c` needed)

## Second Example: Typed Dispatch

```c
#define MIDI2_IMPLEMENTATION
#include "midi2.h"

void on_note_on(uint8_t group, uint8_t channel, uint8_t note,
                uint16_t velocity, uint8_t attr_type,
                uint16_t attr_data, void *ctx) {
    printf("Note On: ch=%d note=%d vel=%u\n", channel, note, velocity);
}

void on_cc(uint8_t group, uint8_t channel, uint8_t index,
           uint32_t value, void *ctx) {
    printf("CC: ch=%d index=%d value=%u\n", channel, index, value);
}

int main(void) {
    midi2_dispatch dp;
    midi2_dispatch_init(&dp);
    dp.on_note_on = on_note_on;
    dp.on_cc = on_cc;

    /* Build a Note On and dispatch it */
    uint32_t w[2];
    midi2_msg_note_on(w, 0, 0, 60, 0xFFFF, 0);
    midi2_dispatch_feed(w, 2, &dp);  /* calls on_note_on */

    return 0;
}
```

Compile with: `gcc -std=c99 -I src example.c -o example` (MIDI2_IMPLEMENTATION includes all `.c` code)

## Third Example: Process + Dispatch Chain

```c
#include "midi2.h"

/* Chain: proc (SysEx reassembly, group filter) -> dispatch (typed callbacks) */

midi2_dispatch dp;
midi2_proc_state proc;
uint8_t sysex7_buf[128];
uint8_t sysex8_buf[256];

void setup(void) {
    midi2_dispatch_init(&dp);
    dp.on_note_on = my_note_on_handler;
    dp.on_cc = my_cc_handler;

    midi2_proc_init(&proc, sysex7_buf, sizeof(sysex7_buf),
                           sysex8_buf, sizeof(sysex8_buf));
    /* Chain: proc feeds dispatch */
    proc.on_ump = midi2_dispatch_feed;
    proc.context = &dp;
}

void on_ump_received(const uint32_t *words, uint8_t word_count) {
    midi2_proc_feed(&proc, words, word_count);
}
```

## What to Read Next

- [Module Reference](Module-Reference.md) -- detailed API for each module
- [Architecture](Architecture.md) -- how the layers fit together
- [Integration Guide](Integration-Guide.md) -- adding midi2 to your project
