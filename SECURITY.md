# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| latest  | Yes       |

## Reporting a Vulnerability

If you discover a security vulnerability, please report it privately:

1. **Do not** open a public issue
2. Email: sauloverissimo@gmail.com
3. Include: description, steps to reproduce, potential impact

You will receive a response within 48 hours. Confirmed vulnerabilities will be patched and disclosed responsibly.

## Scope

midi2 is a data processing library with no network, file, or OS access. Security concerns are limited to:

- Buffer overflows in SysEx reassembly or CI message parsing
- Integer overflows in value scaling or length calculations
- Malformed UMP or MIDI-CI input causing unexpected behavior
- Out-of-bounds reads when parsing variable-length fields (Profiles, PE data)

## Mitigations

- All 232 tests run under AddressSanitizer and UndefinedBehaviorSanitizer in CI
- Input length is validated before accessing message fields
- All multi-byte field access uses explicit bounds checks
- No dynamic memory allocation eliminates use-after-free and double-free classes
