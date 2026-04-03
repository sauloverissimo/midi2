# Contributing to midi2

Thank you for your interest in contributing to midi2.

## Reporting Issues

Open an issue with:
- What you expected
- What happened
- Minimal code to reproduce
- Platform and compiler version

## Code Contributions

1. Fork the repository
2. Create a branch (`git checkout -b fix/description`)
3. Make your changes
4. Run tests: `make test`
5. Verify zero warnings with strict flags:
   ```bash
   make CC=gcc test
   make CC=clang test
   ```
6. Run sanitizers if available:
   ```bash
   make CC="gcc -fsanitize=address,undefined" test
   ```
7. Open a pull request with a clear description

## Code Style

- **C99 strict** (`-std=c99 -Wall -Wextra -Wpedantic`, zero warnings)
- **No dynamic allocation** -- no `malloc`, `free`, or hidden heap usage
- **Caller-provided storage** -- all state in structs the caller allocates
- **Error codes** -- functions that can fail return `int` (0 = success, negative = error)
- **Naming**: `midi2_<module>_<action>()` (e.g., `midi2_msg_note_on`, `midi2_ci_build_discovery`)
- **Every new function needs a test** -- no exceptions
- **No OS dependencies** -- only `stdint.h`, `stdbool.h`, `string.h`
- **Comments**: explain _why_, not _what_. The code should be self-evident.

## Module Structure

If adding a new module, follow the established pattern:

- **Header-only** (`.h` with `static inline`) for stateless construct/parse functions
- **Compiled** (`.h` + `.c`) for stateful logic or dispatch
- Add test file: `test/test_midi2_<module>.c`
- Add to `Makefile` build and test targets
- Add binary to `.gitignore`
- Add to CI workflow (`.github/workflows/ci.yml`) for MSVC and ARM targets

## Testing

```bash
make test                                               # gcc, all 252 tests
make CC=clang test                                      # clang
make CC="gcc -fsanitize=address,undefined" test          # sanitizers
make CC="gcc -m32" test                                  # 32-bit verification
make clean                                               # remove binaries
```

CI runs 11 jobs across desktop (gcc, clang, MSVC, macOS, 32-bit, sanitizers) and embedded (ARM Cortex-M, AArch64, RISC-V, ESP32, AVR) targets.

## Spec References

midi2 implements the MIDI Association specifications. When adding or modifying message handling, always reference the spec section:

- **UMP messages**: M2-104-UM v1.1.2 (table and section numbers in code comments)
- **MIDI-CI messages**: M2-101-UM v1.2
- **Value scaling**: M2-115-U v1.0.2

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
