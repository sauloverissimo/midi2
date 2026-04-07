#!/usr/bin/env bash
# amalgamate.sh -- Generate single-header midi2.h from multi-module sources
# Usage: ./tools/amalgamate.sh [output_file]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$SCRIPT_DIR/../src"
OUT="${1:-$SRC/midi2.h}"
VER=$(grep -o '"version".*"[0-9][^"]*"' "$SCRIPT_DIR/../library.json" 2>/dev/null \
  | grep -o '[0-9][^"]*' || echo "dev")

# Strip boilerplate from a source file (.h or .c):
# - MIT license block, include guards, internal #includes, extern "C" wrappers
strip() {
  awk '
    /^\/\*/ && !ld { il=1 }
    il { if (/^ \*\//) { il=0; ld=1 }; next }
    /^#(ifndef|define) MIDI2_[A-Z_]*_H$/ { next }
    /^#endif \/\* MIDI2_[A-Z_]*_H \*\/$/ { next }
    /^#include.*"midi2_/ { next }
    /^#include <(stdint|stdbool|string)\.h>/ { next }
    /^#ifdef __cplusplus$/ { getline; getline; next }
    { lines[++n] = $0 }
    END {
      while (n > 0 && lines[n] == "") n--
      if (n >= 2 && lines[n] == "#endif" && lines[n-1] == "}") n -= 2
      while (n > 0 && lines[n] == "") n--
      for (i = 1; i <= n; i++) print lines[i]
    }
  ' "$1"
}

section() { printf '\n/* == %s %s */\n\n' "$1" "$(printf '=%.0s' $(seq 1 $((68 - ${#1}))))" ; }

# Header order: bases first, then dependents
HEADERS=(midi2_msg midi2_ci_msg midi2_dispatch midi2_proc midi2_conv midi2_ci_dispatch midi2_ci)
SOURCES=(midi2_dispatch midi2_proc midi2_conv midi2_ci_dispatch midi2_ci)

{
  # Extract license from first source file
  awk '/^\/\*/{p=1} p{print} /^ \*\//{exit}' "$SRC/midi2_msg.h"

  cat <<EOF

/* Auto-generated from midi2 v${VER} -- $(date -u +%Y-%m-%d)
 * https://github.com/sauloverissimo/midi2
 *
 * Usage:
 *   #include "midi2.h"
 *   // In exactly ONE .c file:
 *   #define MIDI2_IMPLEMENTATION
 *   #include "midi2.h"
 */

#ifndef MIDI2_H
#define MIDI2_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
EOF

  for m in "${HEADERS[@]}"; do
    section "$m"
    strip "$SRC/${m}.h"
  done

  printf '\n#ifdef MIDI2_IMPLEMENTATION\n'

  for m in "${SOURCES[@]}"; do
    section "$m (impl)"
    strip "$SRC/${m}.c"
  done

  cat <<'EOF'

#endif /* MIDI2_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* MIDI2_H */
EOF
} > "$OUT"

echo "Generated $OUT ($(wc -l < "$OUT") lines, v${VER})"

# Generate midi2.c companion file
OUT_C="${OUT%.h}.c"
{
  # Extract license from first source file
  awk '/^\/\*/{p=1} p{print} /^ \*\//{exit}' "$SRC/midi2_msg.h"

  cat <<EOF

/* Auto-generated from midi2 v${VER} -- $(date -u +%Y-%m-%d)
 * https://github.com/sauloverissimo/midi2
 *
 * Compile this file to get all midi2 symbols.
 * Include midi2.h in your headers for declarations.
 */

#define MIDI2_IMPLEMENTATION
#include "midi2.h"
EOF
} > "$OUT_C"

echo "Generated $OUT_C ($(wc -l < "$OUT_C") lines, v${VER})"
