# Integration Guide

midi2 is published on the Arduino Library Manager, the PlatformIO Registry, and the ESP Component Registry. CMake projects pick it up via `find_package` or `FetchContent`. Zephyr applications add it as a west module.

Pick the path that matches your build flow.

## Arduino IDE / arduino-cli

```bash
arduino-cli lib update-index
arduino-cli lib install midi2
```

In the IDE, search `midi2` in the Library Manager and click Install. Two reference sketches appear under **File > Examples > midi2**: `basic-usage` and `ci-discovery`. Sketch include:

```cpp
#include <midi2.h>
```

## PlatformIO

```ini
lib_deps = sauloverissimo/midi2 @ ^0.6.0
```

The Registry resolves the modular `src/*.c` layout via `library.json` (`srcDir = src`).

## ESP-IDF (Component Manager)

`main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=5.0"
  sauloverissimo/midi2: ">=0.6.0"
```

`idf.py reconfigure` drops the component into `managed_components/midi2/`. ESP-IDF picks the directory up as a regular component because the top-level `CMakeLists.txt` detects `ESP_PLATFORM` and routes to `idf_component_register` with the modular `src/midi2_*.c` set.

## Zephyr (west module)

Add midi2 to your application's `west.yml`:

```yaml
manifest:
  projects:
    - name: midi2
      url: https://github.com/sauloverissimo/midi2
      revision: v0.6.0
      path: modules/lib/midi2
```

Enable in `prj.conf`:

```
CONFIG_MIDI2=y
```

`west build` picks up the module via `zephyr/module.yml` and compiles the modular `src/midi2_*.c` set under Zephyr's CMake. midi2 stays at the message layer; pair with the Zephyr USB MIDI 2.0 device class (`CONFIG_USBD_MIDI2`) for USB UMP I/O, or with the Network MIDI 2.0 stack for IP transport. A flash-ready example for the Raspberry Pi Pico lives under [`examples/rpi-pico-device-zephyr/`](https://github.com/sauloverissimo/midi2/tree/main/examples/rpi-pico-device-zephyr).

## CMake (find_package + FetchContent)

```cmake
if(NOT TARGET midi2)
  find_package(midi2 0.6.0 QUIET CONFIG)
  if(NOT midi2_FOUND)
    include(FetchContent)
    FetchContent_Declare(midi2
      GIT_REPOSITORY https://github.com/sauloverissimo/midi2.git
      GIT_TAG        v0.6.0)
    FetchContent_MakeAvailable(midi2)
  endif()
endif()

target_link_libraries(my_target PRIVATE midi2::midi2)
```

Three layers of fallback in this order:
1. Parent project already declared a `midi2` target (`add_subdirectory` / FetchContent of a parent that pulled it).
2. System install / vcpkg / conan.
3. FetchContent from GitHub.

## Single-header (stb-style)

For projects that prefer dropping a single header into their tree:

```c
#define MIDI2_IMPLEMENTATION
#include "midi2.h"
```

`dist/midi2.h` is the amalgamated header. Use it in exactly one `.c` file.

## Manual vendor (amalgam pair)

Download `dist/midi2.h` and `dist/midi2.c` from the [release page](https://github.com/sauloverissimo/midi2/releases). Drop both into your tree. Compile `midi2.c` alongside the project.

```c
#include "midi2.h"
```

Works with Arduino, PlatformIO, ESP-IDF, Zephyr, CMake, Make, plain `gcc`.

## Module-by-module (selective)

For finer control, copy individual files from `src/`:

```
midi2_msg.h          Always needed. Header-only.
  +- midi2_dispatch  Typed UMP callbacks.
  +- midi2_proc      SysEx reassembly, group filtering, fragmenters.
  +- midi2_conv      MIDI 1.0 byte stream to UMP.
  +- midi2_ci_msg    MIDI-CI message construction and parsing.
       +- midi2_ci_dispatch  Typed CI callbacks.
       +- midi2_ci           Convenience CI responder.
```

Add `src/` to the include path. Compile only the modules you use.

## Project layout

```
midi2/
  src/      Modular sources (Arduino IDE auto-compiles, PlatformIO via srcDir)
  dist/     Amalgamated midi2.{h,c} (single-header / vendor pair / CMake target)
  zephyr/   west module manifest (module.yml, CMakeLists.txt, Kconfig)
  tools/    amalgamate.sh (regenerates dist/ from src/)
  test/     350 tests across 8 suites
```

## Which modules to include

| Use case | Pick |
|---|---|
| Just build/parse UMP messages | `midi2_msg.h` (header-only, zero cost) |
| Typed UMP callbacks | + `midi2_dispatch.h/.c` |
| SysEx reassembly, group filtering, Stream/SysEx8 fragmenters | + `midi2_proc.h/.c` |
| Receive MIDI 1.0 Serial/DIN | + `midi2_conv.h/.c` |
| Build/parse MIDI-CI messages | + `midi2_ci_msg.h` (header-only) |
| Typed CI callbacks | + `midi2_ci_dispatch.h/.c` |
| Convenience CI responder (Discovery, Profiles, PE, PI) | + `midi2_ci.h/.c` |

The amalgamated `dist/midi2.{h,c}` carries every module; package-manager paths above always pull the full set.
