/*
 * Smoke for the CMake-imported midi2::midi2 target.
 *
 * Builds a NoteOn UMP, parses the note number back, and exits 0 when
 * the round trip is consistent. The CI job both compiles and runs this
 * binary, so a broken find_package config (missing include dirs,
 * missing library, ABI mismatch) shows up immediately.
 */
#include "midi2.h"

#include <stdio.h>

int main(void) {
    uint32_t w[2];
    midi2_msg_note_on(w, /*group*/ 0, /*channel*/ 0,
                      /*note*/ 60, /*velocity16*/ 0xC000,
                      /*attr_type*/ 0, /*attr_data*/ 0);

    if ((w[0] & 0xF0000000u) != 0x40000000u) {
        fprintf(stderr, "midi2_consumer: MT bits wrong (0x%08X)\n", w[0]);
        return 1;
    }
    if (midi2_msg_get_note(w) != 60) {
        fprintf(stderr, "midi2_consumer: note round-trip wrong (%u)\n",
                (unsigned)midi2_msg_get_note(w));
        return 1;
    }
    if (midi2_msg_get_velocity(w) != 0xC000u) {
        fprintf(stderr, "midi2_consumer: vel round-trip wrong (0x%X)\n",
                (unsigned)midi2_msg_get_velocity(w));
        return 1;
    }

    printf("midi2_consumer: OK (NoteOn 60 vel 0xC000 round-trip clean)\n");
    return 0;
}
