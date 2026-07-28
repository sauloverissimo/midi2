/* Host-side unit test for the UMP catalog (gcc, no AVR needed).
 *   gcc -std=c99 -Wall -Wextra -I../src -I../../../src test_catalog.c ../src/catalog.c -o test_catalog && ./test_catalog
 *
 * Invariants: every entry builds, its word count matches the MT size table
 * (a mismatch would make the TX pump split messages across USB packets),
 * and every defined M2-104 message type category is covered.
 */
#include <assert.h>
#include <stdio.h>
#include "catalog.h"
#include "midi2_msg.h"

int main(void) {
    catalog_msg_t m;
    uint32_t      mt_seen = 0;

    assert(midi2_catalog_count() == 58);

    for (uint32_t i = 0; i < midi2_catalog_count(); i++) {
        uint8_t n = midi2_catalog_build(i, &m);
        uint8_t mt = (uint8_t)(m.w[0] >> 28);
        assert(n >= 1 && n <= 4);
        assert(n == midi2_msg_word_count(mt));
        mt_seen |= (uint32_t)1 << mt;
    }

    /* all defined categories: 0x0,0x1,0x2,0x3,0x4,0x5,0xD,0xF */
    assert((mt_seen & ((1u<<0x0)|(1u<<0x1)|(1u<<0x2)|(1u<<0x3)|(1u<<0x4)|(1u<<0x5)|(1u<<0xD)|(1u<<0xF)))
           == ((1u<<0x0)|(1u<<0x1)|(1u<<0x2)|(1u<<0x3)|(1u<<0x4)|(1u<<0x5)|(1u<<0xD)|(1u<<0xF)));

    /* out of range refuses */
    assert(midi2_catalog_build(midi2_catalog_count(), &m) == 0);

    puts("test_catalog: all assertions passed");
    return 0;
}
