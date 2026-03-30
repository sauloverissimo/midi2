# Integration Guide

## Adding midi2 to Your Project

midi2 is designed to be vendorized -- copy the `src/` files into your project tree. No build system required, no package manager dependency.

### Option 1: Copy source files

```bash
cp midi2/src/midi2_msg.h       your_project/lib/midi2/
cp midi2/src/midi2_dispatch.*  your_project/lib/midi2/
cp midi2/src/midi2_proc.*      your_project/lib/midi2/
# ... add whichever modules you need
```

Add `lib/midi2/` to your include path. Compile the `.c` files with your project.

### Option 2: Git submodule

```bash
git submodule add https://github.com/sauloverissimo/midi2.git lib/midi2
```

### Option 3: PlatformIO

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

## CMake Example

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

## ESP-IDF Example (CMakeLists.txt component)

```cmake
idf_component_register(
    SRCS "midi2_dispatch.c" "midi2_proc.c" "midi2_ci_dispatch.c" "midi2_ci.c" "midi2_conv.c"
    INCLUDE_DIRS "."
)
```

## Arduino IDE

Copy the `src/` files into your Arduino library folder. The `.h` files include `extern "C"` guards, so they work directly from C++ sketches.

## Makefile (bare metal)

```makefile
MIDI2_SRC = midi2_dispatch.c midi2_proc.c midi2_ci_dispatch.c midi2_ci.c midi2_conv.c
MIDI2_OBJ = $(MIDI2_SRC:.c=.o)

%.o: %.c
    $(CC) -std=c99 -I. -c $< -o $@
```
