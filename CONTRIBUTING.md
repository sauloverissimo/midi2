# Contributing to midi2

## Reporting issues

Open an issue with:
- What you expected
- What happened
- Minimal code to reproduce

## Code contributions

1. Fork the repository
2. Create a branch (`git checkout -b fix/description`)
3. Make your changes
4. Run tests: `make test`
5. Ensure zero warnings: `make CC=gcc test && make CC=clang test`
6. Open a pull request

## Code style

- C99 strict (`-std=c99 -Wall -Wextra -Wpedantic`)
- No dynamic allocation (malloc/free)
- All state must be caller-provided
- Functions that can fail return error codes
- Naming: `midi2_<module>_<action>()`
- Every new function needs a test

## Testing

```bash
make test           # run all 77 tests
make CC=clang test  # test with clang
make clean          # remove binaries
```

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
