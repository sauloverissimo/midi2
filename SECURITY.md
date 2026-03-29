# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.1.x   | Yes       |

## Reporting a Vulnerability

If you discover a security vulnerability, please report it privately:

1. **Do not** open a public issue
2. Email: sauloverissimo@gmail.com
3. Include: description, steps to reproduce, potential impact

You will receive a response within 48 hours. Confirmed vulnerabilities will be patched and disclosed responsibly.

## Scope

midi2 is a data processing library with no network, file, or OS access. Security concerns are limited to:

- Buffer overflows in SysEx reassembly
- Integer overflows in value scaling
- Malformed UMP input causing unexpected behavior
