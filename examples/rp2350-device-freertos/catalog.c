#include "catalog.h"
#include "midi2_msg.h"

/* Group 0 is the only advertised group (Function Block numGroups=1). */
#define CAT_GROUP   0
#define CAT_CHANNEL 0

uint32_t midi2_catalog_count(void) { return 1; }  /* grows as entries are added */

uint8_t midi2_catalog_build(uint32_t idx, catalog_msg_t *out) {
  switch (idx) {
    case 0: out->w[0] = midi2_msg_noop(); out->n = 1; break;
    default: out->n = 0; break;
  }
  return out->n;
}
