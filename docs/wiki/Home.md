# midi2 Wiki

Portable MIDI 2.0 infrastructure: C99, zero dependencies, zero allocation. Runs from AVR to desktop.

## Contents

### Getting Started
- [Getting Started](Getting-Started.md): quick setup, first build, basic usage
- [Integration Guide](Integration-Guide.md): Arduino LM, PlatformIO, ESP-IDF, Zephyr, CMake, vendor

### Architecture
- [Architecture](Architecture.md): 4-layer model, module boundaries, data flow
- [Module Reference](Module-Reference.md): all 7 modules with their public API surface

### MIDI 2.0 specifications
- [Spec Guide](Spec-Guide.md): coverage map of every MIDI 2.0 specification
- [UMP Message Reference](UMP-Message-Reference.md): every UMP message type with bit layouts
- [MIDI-CI Message Reference](CI-Message-Reference.md): every CI message with field descriptions

### Validation
- [Hardware Validation](Hardware-Validation.md): test results on real hardware

### Contributing
- [Design Decisions](Design-Decisions.md): key architectural choices and their rationale

## Quick links

- **Repository:** [github.com/sauloverissimo/midi2](https://github.com/sauloverissimo/midi2)
- **Latest release:** [v0.7.0](https://github.com/sauloverissimo/midi2/releases/latest)
- **License:** MIT
- **Distribution:** Arduino Library Manager, PlatformIO Registry, ESP-IDF Component Manager, Zephyr west module, CMake `find_package` / `FetchContent`
- **Tests:** 379 passing across 8 suites, 12 CI jobs (gcc / clang / MSVC / Apple clang / x86 32-bit / ASan + UBSan / ARM Cortex-M / Cortex-A / RISC-V / AVR / ESP32 / Zephyr native_sim) plus a CMake matrix on Linux + macOS + Windows
- **Spec coverage:** M2-104-UM v1.1.2 (UMP) 100%, M2-101-UM v1.2 (MIDI-CI) 100%
