/* Device-side UMP Stream responder: answers Endpoint Discovery, Function
 * Block Discovery and Stream Configuration requests with this recipe's
 * identity. Replies go out through midi2duino_write().
 */
#ifndef STREAM_RESPONDER_H
#define STREAM_RESPONDER_H

#include "midi2_dispatch.h"

/* Wire the stream callbacks into an initialized dispatch. */
void stream_responder_attach(midi2_dispatch *dp);

#endif
