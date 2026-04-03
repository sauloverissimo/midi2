# Integration Guide

## Adding midi2 to Your Project

midi2 is designed to be vendorized -- copy into your project tree. No build system required, no package manager dependency.

### Option 1: Single-header (recommended)

Copy one file: `src/midi2.h`. All 7 modules in a single header.

```c
// In any file -- declarations + inline functions
#include "midi2.h"

// In exactly ONE .c file -- generates the implementation
#define MIDI2_IMPLEMENTATION
#include "midi2.h"
```

This follows the same single-header pattern used by SQLite (amalgamation), stb, cJSON, and cmidi2. The file is auto-generated from the multi-module sources via `tools/amalgamate.sh`.

### Option 2: Multi-module (selective inclusion)

Copy individual source files for finer control over what gets compiled:

```bash
cp midi2/src/midi2_msg.h       your_project/lib/midi2/
cp midi2/src/midi2_dispatch.*  your_project/lib/midi2/
cp midi2/src/midi2_proc.*      your_project/lib/midi2/
# ... add whichever modules you need
```

Add `lib/midi2/` to your include path. Compile the `.c` files with your project.

### Option 3: Git submodule

```bash
git submodule add https://github.com/sauloverissimo/midi2.git lib/midi2
```

### Option 4: PlatformIO

`library.json` is included. Add to your `platformio.ini`:

```ini
lib_deps = https://github.com/sauloverissimo/midi2.git
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

### Single-header (any build system)

```c
// midi2_impl.c -- compile this one file
#define MIDI2_IMPLEMENTATION
#include "midi2.h"
```

```bash
gcc -std=c99 -c midi2_impl.c -o midi2.o
gcc -std=c99 your_app.c midi2.o -o your_app
```

### CMake (single-header)

```cmake
add_library(midi2 STATIC midi2_impl.c)
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
    SRCS "midi2_impl.c"
    INCLUDE_DIRS "."
)
```

Where `midi2_impl.c` contains only `#define MIDI2_IMPLEMENTATION` and `#include "midi2.h"`.

### Arduino IDE

Copy `midi2.h` into your Arduino library folder. Create `midi2.cpp`:

```cpp
#define MIDI2_IMPLEMENTATION
#include "midi2.h"
```

The header includes `extern "C"` guards, so it works directly from C++ sketches.

### Makefile (bare metal)

```makefile
midi2.o: midi2_impl.c midi2.h
    $(CC) -std=c99 -I. -c $< -o $@
```
