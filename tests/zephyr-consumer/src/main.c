/*
 * Copyright (c) 2026 Saulo Verissimo
 * SPDX-License-Identifier: MIT
 *
 * midi2 Zephyr smoke consumer. Exercises construction + typed
 * dispatch on a minimal application. Used by CI on native_sim to
 * verify that the Zephyr module integration keeps building and
 * dispatching as expected.
 */

#include <zephyr/kernel.h>
#include <stdio.h>

#include "midi2_msg.h"
#include "midi2_dispatch.h"

static int g_notes_seen = 0;

static void on_note_on(uint8_t group, uint8_t channel, uint8_t note,
                       uint16_t velocity, uint8_t attr_type,
                       uint16_t attr_data, void *ctx) {
        (void)ctx;
        (void)attr_type;
        (void)attr_data;
        printf("midi2: NoteOn g=%u ch=%u n=%u v=%u\n",
               group, channel, note, velocity);
        g_notes_seen++;
}

int main(void)
{
        printf("midi2 Zephyr smoke: start\n");

        uint32_t w[2];
        midi2_msg_note_on(w, /*group*/ 0, /*channel*/ 0,
                          /*note*/ 60, /*velocity*/ 0xC000,
                          /*attr_type*/ 0, /*attr_data*/ 0);

        midi2_dispatch dp;
        midi2_dispatch_init(&dp);
        dp.on_note_on = on_note_on;
        midi2_dispatch_feed(w, 2, &dp);

        if (g_notes_seen != 1) {
                printf("midi2 Zephyr smoke: FAIL expected 1 note, got %d\n",
                       g_notes_seen);
                return 1;
        }

        printf("midi2 Zephyr smoke: PASS\n");
        return 0;
}
