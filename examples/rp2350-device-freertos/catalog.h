#ifndef RP2350_CATALOG_H
#define RP2350_CATALOG_H
#include <stdint.h>

/* One UMP message: n words (1..4) in w[]. Matches pipeline.h ump_msg_t on purpose. */
typedef struct {
  uint8_t  n;
  uint32_t w[4];
} catalog_msg_t;

/* Total number of catalog entries. */
uint32_t midi2_catalog_count(void);

/* Build entry `idx` (0..count-1) into `out`. Returns out->n, or 0 if idx out of range. */
uint8_t midi2_catalog_build(uint32_t idx, catalog_msg_t *out);

#endif
