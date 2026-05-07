/* midi2.h, umbrella header for the modular C99 source layout under src/.
 *
 * Including this single header pulls in the full midi2 public API. The
 * implementation lives in the per-module .c files in this directory
 * (midi2_proc.c, midi2_dispatch.c, midi2_conv.c, midi2_ci.c,
 * midi2_ci_dispatch.c). Arduino IDE / arduino-cli compile every .c in
 * src/ automatically when this library is installed; PlatformIO uses
 * the same layout via library.json's srcDir = src.
 *
 * For consumers that prefer the stb-style single-file drop-in,
 * dist/midi2.h + dist/midi2.c carry the amalgamated form (header-only
 * declarations + a single TU that defines MIDI2_IMPLEMENTATION). Both
 * forms expose the same symbols and headers; the only difference is
 * how the implementation is split on disk.
 */
#pragma once

#include "midi2_msg.h"
#include "midi2_proc.h"
#include "midi2_dispatch.h"
#include "midi2_conv.h"
#include "midi2_ci.h"
#include "midi2_ci_dispatch.h"
#include "midi2_ci_msg.h"
