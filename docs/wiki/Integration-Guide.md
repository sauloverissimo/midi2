# Integration Guide

## Adding midi2 to Your Project

midi2 is designed to be vendorized -- copy into your project tree. No build system required, no package manager dependency.

### Option 1: Pair (recommended)

Copy two files: `dist/midi2.h` + `dist/midi2.c`. Your build system compiles `midi2.c` alongside your code. No special defines needed.

```c
// In your code -- just include the header
#include "midi2.h"
```

This works naturally with platforms that auto-compile `.c` files (Arduino, Teensy, PlatformIO, ESP-IDF components).

### Option 2: Single-header (alternative)

Copy one file: `dist/midi2.h`. Use the stb-style pattern:

```c
// In any file -- declarations + inline functions
#include "midi2.h"

// In exactly ONE .c file -- generates the implementation
#define MIDI2_IMPLEMENTATION
#include "midi2.h"
```

### Option 3: Multi-module (selective inclusion)

Copy individual source files from `src/` for finer control over what gets compiled:

```bash
cp midi2/src/midi2_msg.h       your_project/lib/midi2/
cp midi2/src/midi2_dispatch.*  your_project/lib/midi2/
cp midi2/src/midi2_proc.*      your_project/lib/midi2/
# ... add whichever modules you need
```

Add `lib/midi2/` to your include path. Compile the `.c` files with your project.

### Option 4: Git submodule

```bash
git submodule add https://github.com/sauloverissimo/midi2.git lib/midi2
```

### Option 5: PlatformIO

`library.json` is included. Add to your `platformio.ini`:

```ini
lib_deps = https://github.com/sauloverissimo/midi2.git
```

## Project Structure

```
midi2/
  src/      Multi-module sources (development, PlatformIO)
  dist/     Amalgamated midi2.h + midi2.c (vendoring)
  tools/    amalgamate.sh (generates dist/ from src/)
  test/     252 tests across 8 test suites
```

## Which Modules to Include

Pick only what you need:

| Use case | Include these |
|----------|-------------|
| Just build/parse UMP messages | `midi2_msg.h` (header-only, zero cost) |
| Typed UMP callbacks | + `midi2_dispatch.h/.c` |
| SysEx reassembly, group filtering | + `midi2_proc.h/.c` |
| Receive MIDI 1.0 Serial/DIN | + `midi2_conv.h/.c` |
| Build/parse MIDI-CI messages | + `midi2_ci_msg.h` (header-only) |
| Typed CI callbacks | + `midi2_ci_dispatch.h/.c` |
| Quick CI auto-responder | + `midi2_ci.h/.c` |

## Build Examples

### Pair (any build system)

```bash
# Just compile midi2.c with your project
gcc -std=c99 -c midi2.c -o midi2.o
gcc -std=c99 your_app.c midi2.o -o your_app
```

### CMake (pair)

```cmake
add_library(midi2 STATIC midi2.c)
target_include_directories(midi2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(midi2 PRIVATE c_std_99)
```

### CMake (multi-module)

```cmake
add_library(midi2 STATIC
    lib/midi2/midi2_dispatch.c
    lib/midi2/midi2_proc.c
    lib/midi2/midi2_ci_dispatch.c
    lib/midi2/midi2_ci.c
    lib/midi2/midi2_conv.c
)
target_include_directories(midi2 PUBLIC lib/midi2)
target_compile_features(midi2 PRIVATE c_std_99)
```

### ESP-IDF component

```cmake
idf_component_register(
    SRCS "midi2.c"
    INCLUDE_DIRS "."
)
```

### Arduino / Teensy

Copy `midi2.h` + `midi2.c` into your project or library folder. The build system compiles `midi2.c` automatically. The header includes `extern "C"` guards, so it works directly from C++ sketches.

### Makefile (bare metal)

```makefile
midi2.o: midi2.c midi2.h
    $(CC) -std=c99 -I. -c $< -o $@
```
