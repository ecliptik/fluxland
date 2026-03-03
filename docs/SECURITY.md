# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in fluxland, please report it
responsibly. **Do not open a public GitHub issue for security
vulnerabilities.**

Instead, please email the maintainers directly or use GitHub's private
vulnerability reporting feature:

1. Go to the repository's **Security** tab
2. Click **Report a vulnerability**
3. Provide a description of the issue, steps to reproduce, and any
   potential impact

We will acknowledge receipt within 48 hours and aim to provide a fix or
mitigation within 7 days for critical issues.

## Supported Versions

| Version | Supported |
|---------|-----------|
| 1.0.x   | Yes       |
| < 1.0   | No        |

## Security Measures

Fluxland implements several security hardening measures:

### Input Validation
- **`safe_atoi()`** — All integer parsing uses bounds-checked
  conversion; raw `atoi()`/`atol()` are never used.
- **Configuration validation** — The `--check-config` CLI flag
  validates all configuration files and reports errors without
  starting the compositor.
- **Config directory validation** — Ownership and permission checks
  on config directories, with path traversal (`..`) rejection.

### File System
- **`fopen_nofollow()`** — Config files are opened with `O_NOFOLLOW`
  to prevent symlink attacks.
- **Startup script safety** — The startup file uses `fstat()` after
  `open()` to avoid TOCTOU races.

### IPC Socket
- **`SO_PEERCRED` authentication** — Only connections from the same
  UID as the compositor are accepted.
- **Restrictive socket permissions** — The socket is created with
  `umask(0077)` and explicitly set to mode `0600`.
- **`XDG_RUNTIME_DIR` validation** — Ownership and permission checks
  on the runtime directory.
- **Buffer limits** — IPC read buffers are capped at 64 KiB to
  prevent memory exhaustion.
- **Client limits** — Maximum 128 concurrent IPC clients.
- **`--ipc-no-exec` flag** — Disables the `Exec` action via IPC to
  prevent command injection in restricted environments.

### Child Processes
- **Environment variable denylist** — Sensitive variables are
  stripped from child process environments.
- **Double-fork pattern** — All 5 exec paths use double-fork to
  avoid SIGCHLD racing with wlroots.

### Compiler Hardening
- `-fstack-protector-strong` — Stack buffer overflow detection.
- `-D_FORTIFY_SOURCE=2` — Runtime buffer overflow checks.
- Full RELRO — Prevents GOT overwrite attacks.

### Build-Time Security Analysis
- Address Sanitizer (`-Dasan=true`)
- Undefined Behavior Sanitizer (`-Dubsan=true`)
- 4 fuzz targets (rcparser, keybind, style, menu) using libFuzzer

## Security Audit History

The v1.0.0 release included a comprehensive security audit that
identified and resolved 36 findings across the codebase:

- Buffer overflow risks from `sprintf` replaced with `snprintf`
- Missing NULL checks after memory allocation
- Integer overflow guards in buffer size calculations
- Input validation for all user-configurable numeric values
- Symlink attack prevention on config file reads
- IPC socket hardening (permissions, authentication, buffer limits)
- Environment sanitization for child processes
- Path traversal prevention in config directory resolution

All findings were verified resolved before release.
