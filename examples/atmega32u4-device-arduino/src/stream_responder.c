#include <string.h>
#include "stream_responder.h"
#include "midi2_msg.h"
#include <midi2duino.h>
#include "board.h"

/* Identity: midi2.diy defaults (educational SysEx prefix 0x7D) */
#define IDENT_MANUFACTURER  0x7D0000UL
#define IDENT_FAMILY        0x0001
#define IDENT_MODEL         0x0002   /* distinct from the baremetal recipe (0x0001) */
#define IDENT_VERSION       0x00010000UL   /* 1.0 */
#define FB_NUM_MAIN         0

/* A dropped Stream reply breaks host discovery, so pump the transport until
 * the packet fits. Bounded so a host that stops reading cannot hang the loop
 * (the pump only moves rings to endpoints; it never re-enters the processor). */
static void send4(const uint32_t *w) {
    for (uint16_t tries = 0; tries < 1000; tries++) {
        if (midi2duino_write(w, 4))
            return;
        midi2duino_task();
    }
}

/* multi-packet text notification, 14 bytes per UMP (13 for FB name) */
static void send_text(uint16_t which, uint8_t fb_num, const char *text) {
    uint32_t w[4];
    uint8_t  len = (uint8_t)strlen(text);
    uint8_t  chunk = (which == MIDI2_STREAM_FB_NAME) ? 13 : 14;
    uint8_t  off = 0;

    while (off < len) {
        uint8_t n = (uint8_t)((len - off > chunk) ? chunk : (len - off));
        uint8_t format;
        if (len <= chunk)            format = 0;          /* complete */
        else if (off == 0)           format = 1;          /* start    */
        else if (off + n >= len)     format = 3;          /* end      */
        else                         format = 2;          /* continue */

        if (which == MIDI2_STREAM_ENDPOINT_NAME)
            midi2_msg_stream_endpoint_name(w, format, (const uint8_t*)text + off, n);
        else if (which == MIDI2_STREAM_PRODUCT_INSTANCE_ID)
            midi2_msg_stream_product_id(w, format, (const uint8_t*)text + off, n);
        else
            midi2_msg_stream_fb_name(w, format, fb_num, (const uint8_t*)text + off, n);
        send4(w);
        off = (uint8_t)(off + n);
    }
}

static void send_fb_info(void) {
    uint32_t w[4];
    /* 1 FB, bidirectional (M2-104: a CI responder FB must be bidirectional),
     * groups 0..0, CI 1.2, protocol MIDI 1.0 + 2.0. The catalog emits
     * SysEx8/MDS, so max_sysex8_streams is honestly 1. */
    midi2_msg_stream_fb_info(w, true, FB_NUM_MAIN,
                             0x03 /* bidir */, 0x03 /* ui: both */,
                             0, 1,
                             2 /* CI 1.2 */, 1 /* sysex8: catalog emits */,
                             0x03 /* both protocols */);
    send4(w);
}

static void on_ep_discovery(uint8_t ver_hi, uint8_t ver_lo, uint8_t filter, void *ctx) {
    uint32_t w[4];
    (void)ver_hi; (void)ver_lo; (void)ctx;

    if (filter & 0x01) {
        midi2_msg_stream_endpoint_info(w, 1, 1, true /* static FBs */, 1,
                                       true /* midi2 */, true /* midi1 */,
                                       false, false);
        send4(w);
    }
    if (filter & 0x02) {
        midi2_msg_stream_device_identity(w, IDENT_MANUFACTURER,
                                         IDENT_FAMILY, IDENT_MODEL,
                                         IDENT_VERSION);
        send4(w);
    }
    if (filter & 0x04)
        send_text(MIDI2_STREAM_ENDPOINT_NAME, 0, BOARD_NAME);
    if (filter & 0x08)
        send_text(MIDI2_STREAM_PRODUCT_INSTANCE_ID, 0, "AT-40D1");
    if (filter & 0x10) {
        midi2_msg_stream_config_notify(w, 0x02 /* MIDI 2.0 */, false, false);
        send4(w);
    }
}

static void on_fb_discovery(uint8_t fb_num, uint8_t filter, void *ctx) {
    (void)ctx;
    if (fb_num != FB_NUM_MAIN && fb_num != 0xFF)
        return;
    if (filter & 0x01)
        send_fb_info();
    if (filter & 0x02)
        send_text(MIDI2_STREAM_FB_NAME, FB_NUM_MAIN, "Main");
}

static void on_config_request(uint8_t protocol, bool rx_jr, bool tx_jr, void *ctx) {
    uint32_t w[4];
    (void)rx_jr; (void)tx_jr; (void)ctx;

    if (protocol != 0x01 && protocol != 0x02)
        protocol = 0x02;
    /* JR timestamps stay off: acknowledge protocol, decline JR */
    midi2_msg_stream_config_notify(w, protocol, false, false);
    send4(w);
}

void stream_responder_attach(midi2_dispatch *dp) {
    dp->on_endpoint_discovery = on_ep_discovery;
    dp->on_fb_discovery       = on_fb_discovery;
    dp->on_config_request     = on_config_request;
}
