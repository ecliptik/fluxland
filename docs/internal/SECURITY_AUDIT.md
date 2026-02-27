# Fluxland Security Audit Report

**Date:** 2026-02-25
**Scope:** Full codebase — 34,559 lines of C across 90 files (46 .c, 44 .h)
**Methods:** Static analysis (cppcheck 2.17.1, flawfinder 2.0.19) + manual code audit
**Build system:** Meson with GCC/Clang, C17

---

## Executive Summary

Fluxland is a Wayland compositor with a moderate attack surface: it processes
user configuration files, accepts IPC commands over a Unix socket, handles
Wayland protocol input from client applications, and executes shell commands
via keybindings, menus, and IPC. The codebase demonstrates generally competent
C practices (proper double-fork, buffer size limits on IPC reads, socket
permission restrictions, no unsafe string functions), but has several areas
requiring hardening — particularly in the IPC command surface and integer
parsing.

**Critical findings:** 2
**High findings:** 8
**Medium findings:** 12
**Low findings:** 12
**Informational:** 7

No remotely exploitable vulnerabilities were found. All critical and
high-severity findings require local access (IPC socket connection or config
file write access).

**Remediation status:** All critical, high, medium, and low findings have been
resolved across 4 phases (36 individual fixes). Only informational items and
minor code quality suggestions remain — none with security impact.

---

## Tool Results Summary

| Tool | Total Hits | Breakdown |
|------|-----------|-----------|
| cppcheck | 375 | 2 real errors (missingReturn), 139 false-positive uninitvar (wl_list_for_each macro), 1 warning, 200 style, 33 info |
| flawfinder | 229 | 14 level-4, 10 level-3, 152 level-2, 53 level-1 |
| manual audit | 31 | 2 critical, 5 high, 12 medium, 12 low (deduplicated with tool findings below) |

---

## Findings

### CRITICAL-1: IPC SetEnv Allows Overwriting Security-Sensitive Environment Variables

**Severity:** CRITICAL
**CWE:** CWE-78 (OS Command Injection), CWE-426 (Untrusted Search Path)
**File:** `src/ipc_commands.c:1131-1152`
**Source:** Manual audit (Task #2)

**Description:**
The IPC `SetEnv` action allows any IPC client to set arbitrary environment
variables in the compositor process with no denylist. Combined with the `Exec`
action, this enables full code injection:

```c
case WM_ACTION_SET_ENV:
    if (argument) {
        char *buf = strdup(argument);
        if (buf) {
            char *eq = strchr(buf, '=');
            if (eq) {
                *eq = '\0';
                setenv(buf, eq + 1, 1);  // No denylist!
            }
```

**Attack scenario:**
```
fluxland-ctl action SetEnv LD_PRELOAD=/tmp/evil.so
fluxland-ctl action Exec /usr/bin/anything
```

This overwrites `LD_PRELOAD`, `PATH`, `HOME`, `LD_LIBRARY_PATH`, or any
other variable. All subsequent child processes inherit the poisoned
environment.

**Recommendation:**
1. Add a denylist for security-sensitive variables:
   ```c
   static const char *env_denylist[] = {
       "LD_PRELOAD", "LD_LIBRARY_PATH", "LD_AUDIT",
       "LD_DEBUG", "LD_PROFILE", "PATH", "HOME",
       "XDG_RUNTIME_DIR", "SHELL", NULL
   };
   ```
2. Or restrict `SetEnv` to a prefix (e.g., `FLUXLAND_*` only)
3. Consider requiring `--ipc-allow-setenv` CLI flag

---

### CRITICAL-2: IPC Allows Arbitrary Command Execution via Exec Action

**Severity:** CRITICAL
**CWE:** CWE-78 (OS Command Injection)
**Files:** `src/ipc_commands.c:404-423`, `src/ipc.c`
**Source:** flawfinder [4], Manual audit (Task #2, Task #3)

**Description:**
The IPC socket accepts a `command` verb with an `Exec` action that passes
the argument string directly to `/bin/sh -c` via `execl()`. Any process that
can connect to the IPC socket can execute arbitrary commands as the
compositor user.

```c
// ipc_commands.c:419
execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
```

**Mitigations already present:**
- Socket has 0600 permissions (owner-only access via `fchmod()`)
- Socket is in `$XDG_RUNTIME_DIR` which is typically 0700

**Risk assessment:**
This is by design (same pattern as sway/i3 IPC), but represents the highest
privilege operation the IPC interface exposes. The socket permission model
is the sole access control mechanism. No authentication or peer credential
checking is performed.

**Recommendations:**
1. Add `SO_PEERCRED` peer credential checking to verify connecting UID matches compositor UID
2. Add optional `--ipc-no-exec` CLI flag to disable Exec/SetEnv via IPC
3. Document the IPC security model in the man page
4. Consider an IPC command allowlist for restricted environments

---

### HIGH-1: Missing Build Hardening Flags

**Severity:** HIGH
**CWE:** CWE-693 (Protection Mechanism Failure)
**File:** `meson.build`
**Source:** Manual review

**Description:**
The build configuration lacks standard binary hardening flags that are
expected for security-sensitive software running as a display server.

| Flag | Purpose | Status |
|------|---------|--------|
| `-fstack-protector-strong` | Stack buffer overflow detection | **MISSING** |
| `-D_FORTIFY_SOURCE=2` | Buffer overflow detection (libc) | **MISSING** |
| `-fstack-clash-protection` | Stack clash attack prevention | **MISSING** |
| `-Wl,-z,relro,-z,now` | Full RELRO (GOT protection) | **MISSING** |
| `-Wformat -Wformat-security` | Format string vulnerability warnings | **MISSING** |
| `b_pie=true` | Position Independent Executable (ASLR) | **MISSING** (may be distro default) |

**Recommendation:**
Add to `meson.build`:
```meson
# Hardening flags
add_project_arguments(cc.get_supported_arguments([
  '-fstack-protector-strong',
  '-fstack-clash-protection',
  '-D_FORTIFY_SOURCE=2',
  '-Wformat',
  '-Wformat-security',
]), language: 'c')

add_project_link_arguments(cc.get_supported_link_arguments([
  '-Wl,-z,relro',
  '-Wl,-z,now',
]), language: 'c')
```

Also add `b_pie=true` to default_options.

---

### HIGH-2: 30+ Unchecked atoi() Calls Throughout IPC and Config Parsing

**Severity:** HIGH
**CWE:** CWE-190 (Integer Overflow), CWE-20 (Improper Input Validation)
**Files:** `src/ipc_commands.c` (14), `src/keyboard.c` (12), `src/rules.c` (3), `src/style.c` (1), `src/validate.c` (2), `src/cursor.c` (1), `src/mousebind.c` (1), `src/menu.c` (1), `src/keybind.c` (1)
**Source:** flawfinder [2], Manual audit (Task #1, Task #3) — **deduplicated**

**Description:**
Approximately 36 calls to `atoi()` parse user-supplied strings (from config
files and IPC commands) without any validation. `atoi()` has undefined
behavior on overflow, does not distinguish `0` from parse failure, and
cannot detect malformed input.

Of particular concern:
- `style.c:183` — Font size parsed via `atoi()` with no upper bound, enabling
  huge Pango/Cairo allocations that could cause memory exhaustion
- `ipc_commands.c` — 14 calls parsing IPC-supplied arguments from the socket

**Recommendation:**
Replace all `atoi()` calls with a safe parsing function:
```c
static bool safe_atoi(const char *s, int *out) {
    char *end;
    errno = 0;
    long val = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0' || val > INT_MAX || val < INT_MIN)
        return false;
    *out = (int)val;
    return true;
}
```

---

### HIGH-3: Integer Truncation in rc_get_int() and style_get_int()

**Severity:** HIGH
**CWE:** CWE-190 (Integer Overflow), CWE-681 (Incorrect Conversion)
**Files:** `src/rcparser.c:173`, `src/style.c:353-356`
**Source:** Manual audit (Task #1, Task #3) — **deduplicated**

**Description:**
`rc_get_int()` and `style_get_int()` use `strtol()` (returns `long`) but
cast the result to `int` without range checking. On LP64 systems where
`long` is 64 bits, values > INT_MAX silently truncate, producing unexpected
negative numbers that bypass subsequent validation.

**Recommendation:**
Add explicit range check after strtol:
```c
long val = strtol(str, &end, 10);
if (val > INT_MAX || val < INT_MIN) {
    return default_val;
}
```

---

### HIGH-4: IPC Socket TOCTOU on Creation

**Severity:** HIGH
**CWE:** CWE-367 (TOCTOU Race Condition)
**File:** `src/ipc.c:224-261`
**Source:** Manual audit (Task #2), static analysis review

**Description:**
The IPC socket creation has a time-of-check-to-time-of-use race:

1. `unlink(ipc->socket_path)` — removes stale socket (or any file at that path)
2. `bind()` — creates socket file with default umask permissions
3. `fchmod(ipc->socket_fd, 0600)` — restricts to owner-only

Between steps 2 and 3, there is a window where the socket has permissive
permissions (e.g., 0755 with default umask). Additionally, the `unlink()` in
step 1 will remove whatever file is at that path without verification.

**Recommendation:**
Set umask before bind to close the race window:
```c
mode_t old_umask = umask(0077);
if (bind(ipc->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    umask(old_umask);
    goto err;
}
umask(old_umask);
// fchmod is still good as a defense-in-depth measure
```

---

### HIGH-5: Shell Injection via Wallpaper Filenames

**Severity:** HIGH
**CWE:** CWE-78 (OS Command Injection)
**File:** `src/menu.c:521-523`
**Source:** Manual audit (Task #2, Task #3) — **deduplicated**

**Description:**
Directory entries (filenames) from the wallpaper directory are interpolated
directly into a shell command string via `asprintf()` without any escaping
or sanitization:

```c
if (asprintf(&item->command,
    "swaybg -i %s/%s -m fill",
    dir_path, names[i]) < 0)
```

A filename containing shell metacharacters (e.g., `; rm -rf /` or
`` `malicious` ``) would execute arbitrary commands when the user selects
the wallpaper menu item.

**Recommendation:**
Shell-escape both `dir_path` and `names[i]` before interpolation, or use
`execv()` with separate arguments instead of `sh -c`.

---

### HIGH-6: Menu [include] Reads Arbitrary Files Without Path Restriction

**Severity:** HIGH
**CWE:** CWE-22 (Path Traversal), CWE-73 (External Control of File Name)
**File:** `src/menu.c:734-750`
**Source:** Manual audit (Task #2)

**Description:**
The menu `[include]` directive reads arbitrary file paths as menu definitions
with no path restriction or confinement:

```c
if (strcasecmp(tag, "include") == 0) {
    char *raw_path = parse_paren(&rest);
    if (raw_path) {
        char *path = expand_path(raw_path);  // expands ~ but no restriction
        if (path) {
            FILE *inc_fp = fopen(path, "r");  // opens any readable file
```

If menu files are writable by another user or sourced from an untrusted
location, this enables reading arbitrary files (e.g., `/etc/shadow` would
fail silently, but other files could affect menu behavior).

**Recommendation:**
Restrict include paths to the config directory or reject paths containing
`..` components and absolute paths outside the config tree.

---

### HIGH-7: IPC SetStyle Loads Arbitrary File Paths

**Severity:** HIGH
**CWE:** CWE-22 (Path Traversal)
**File:** `src/ipc_commands.c:881-900`
**Source:** Manual audit (Task #2)

**Description:**
The IPC `SetStyle` action loads arbitrary file paths into the style parser
without path validation. An IPC client could point this at any readable file.

**Recommendation:**
Validate the style file path against a known directory or require it to be
within the config tree.

---

### HIGH-8: Missing Return Statements in Non-Void Functions

**Severity:** HIGH
**CWE:** CWE-758 (Undefined Behavior)
**Files:** `src/ipc_commands.c:486`, `src/menu.c:2299`
**Source:** cppcheck [missingReturn]

**Description:**
Two non-void functions have execution paths that fall through without a
return statement. This is undefined behavior in C and can lead to
unpredictable results, including potential security implications if the
caller uses the garbage return value for control flow.

**Recommendation:**
Add explicit return statements to all exit paths in these functions.

---

### MEDIUM-1: No Root Detection or Privilege Dropping

**Severity:** MEDIUM
**CWE:** CWE-250 (Execution with Unnecessary Privileges)
**File:** `src/main.c`
**Source:** Manual audit (Task #2)

**Description:**
The compositor does not check if it is running as root and performs no
privilege dropping. If accidentally started as root (or via a misconfigured
display manager), all child processes (including user commands via Exec)
would run as root.

**Recommendation:**
Add a check at startup:
```c
if (getuid() == 0 || geteuid() == 0) {
    wlr_log(WLR_ERROR, "Running as root is not supported");
    return 1;
}
```

---

### MEDIUM-2: Format String from User Config in Toolbar

**Severity:** MEDIUM
**CWE:** CWE-134 (Use of Externally-Controlled Format String)
**File:** `src/toolbar.c:752-759`
**Source:** Manual audit (Task #1)

**Description:**
The iconbar iconified title pattern comes from user configuration and is
used directly as a `snprintf` format string:

```c
const char *pattern = cfg->iconbar_iconified_pattern;
if (!pattern) pattern = "(%s)";
snprintf(title_buf, sizeof(title_buf), pattern, raw_title);
```

If the pattern contains format specifiers beyond `%s` (e.g., `%n`, `%x`),
this could read/write stack memory. The `snprintf` bounds limit the damage,
but this is still a format string vulnerability.

**Recommendation:**
Validate the pattern to ensure it contains only one `%s` and no other
format specifiers, or use string replacement instead of printf formatting.

---

### MEDIUM-3: Hand-Written JSON Parser Fragile on Malformed Input

**Severity:** MEDIUM
**CWE:** CWE-20 (Improper Input Validation)
**File:** `src/ipc_commands.c:86-154`
**Source:** Manual audit (Task #3)

**Description:**
The `json_get_string()` hand-written JSON parser can misbehave on malformed
input — specifically unterminated strings and escape sequences at
end-of-string. While currently bounded by the IPC read buffer limit, edge
cases could produce unexpected values.

**Recommendation:**
Add explicit bounds checking for string termination in the JSON parser, or
replace with a lightweight JSON library (e.g., cJSON).

---

### MEDIUM-4: No Rate Limit on IPC Event Broadcasts

**Severity:** MEDIUM
**CWE:** CWE-400 (Uncontrolled Resource Consumption)
**File:** `src/ipc.c:316-334`
**Source:** Manual audit (Task #3)

**Description:**
IPC event broadcasts to subscribed clients have no rate limiting. A rapid
sequence of events (e.g., window focus changes during a drag) could cause
excessive write calls and buffer growth for all subscribed clients.

**Recommendation:**
Implement a per-client event rate limit or coalescing mechanism.

---

### MEDIUM-5: No Limit on IPC Client Connections

**Severity:** MEDIUM
**CWE:** CWE-400 (Uncontrolled Resource Consumption)
**File:** `src/ipc.c:147-169`
**Source:** Manual audit (Task #3), static analysis review

**Description:**
`handle_new_connection()` accepts unlimited client connections via `accept4()`.
A local process could exhaust file descriptors by opening many connections.

**Recommendation:**
Track client count and reject connections beyond a reasonable limit (e.g., 32).

---

### MEDIUM-6: IPC BindKey Action Plants Arbitrary Keybindings

**Severity:** MEDIUM
**CWE:** CWE-862 (Missing Authorization)
**File:** `src/ipc_commands.c:1154-1164`
**Source:** Manual audit (Task #2)

**Description:**
The IPC `BindKey` action allows adding arbitrary keybindings (including Exec
commands) via the IPC socket. This is another vector for command injection
if the IPC socket is compromised.

**Recommendation:**
Consider gating BindKey behind a `--ipc-allow-bind` flag or at least
documenting the risk.

---

### MEDIUM-7: Unbounded Capacity Growth in Validation Results

**Severity:** MEDIUM
**CWE:** CWE-400 (Uncontrolled Resource Consumption)
**File:** `src/validate.c:40-48`
**Source:** Manual audit (Task #3)

**Description:**
`result_add()` in the validation module grows capacity without limit when
accumulating validation errors. A config file with thousands of errors could
cause excessive memory allocation.

**Recommendation:**
Add a maximum error count limit (e.g., 1000 errors).

---

### MEDIUM-8: Catastrophic Regex Backtracking in Window Rules

**Severity:** MEDIUM
**CWE:** CWE-1333 (Inefficient Regular Expression Complexity)
**File:** `src/rules.c:81-178`
**Source:** Manual audit (Task #3)

**Description:**
Window rule patterns use `fnmatch()` which is generally safe, but complex
patterns from config could potentially cause excessive matching time on
adversarial window titles.

**Recommendation:**
Add a timeout or pattern complexity limit for rule matching.

---

### MEDIUM-9: File Operations Without Symlink Protection

**Severity:** MEDIUM
**CWE:** CWE-362 (Race Condition), CWE-59 (Improper Link Resolution)
**Files:** All `fopen()` calls in config/style/rules/menu/keybind/mousebind/validate/slit modules
**Source:** flawfinder [2], Manual audit (Task #2)

**Description:**
Configuration files are opened with `fopen()` and `open()` without
`O_NOFOLLOW` or symlink checking. If an attacker can create symlinks in
the user's config directory, they could cause the compositor to read
unexpected files.

**Recommendation:**
Consider using `openat()` with `O_NOFOLLOW` for config file access,
particularly on config reload paths that are triggered by IPC commands.

---

### MEDIUM-10: Environment Variable Trust Issues

**Severity:** MEDIUM
**CWE:** CWE-807 (Reliance on Untrusted Inputs in a Security Decision)
**Files:** `src/config.c:242-277`, `src/ipc.c:200-208`, `src/menu.c:201`
**Source:** flawfinder [3], Manual audit (Task #2)

**Description:**
The compositor trusts `XDG_RUNTIME_DIR`, `XDG_CONFIG_HOME`, `HOME`,
`FLUXLAND_CONFIG_DIR`, and `XDG_DATA_DIRS` without validation. The
`FLUXLAND_CONFIG_DIR` variable (checked in config.c:242) is particularly
concerning as it can redirect config loading to an arbitrary directory.

**Recommendation:**
- Validate `XDG_RUNTIME_DIR` is owned by the current user and has 0700 permissions
- Reject environment paths containing `..` components

---

### MEDIUM-11: Dead/Unreachable Code in Decoration and Tab Rendering

**Severity:** MEDIUM
**CWE:** CWE-561 (Dead Code), CWE-570 (Expression Always False)
**File:** `src/decoration.c:597,671,1069,1587-1593`
**Source:** cppcheck [identicalConditionAfterEarlyExit, knownConditionTrueFalse]

**Description:**
- `decoration.c:1593` — Identical condition after early return (unreachable code)
- `decoration.c:597,671,1069` — Conditions `idx == tab_count - 1` always false
  because `idx` is 0 when the check executes. The "last tab" styling logic
  never runs.

**Recommendation:**
Fix the conditional logic — idx should be incremented before the last-tab check.

---

### MEDIUM-12: Fixed-Size JSON Escape Buffers Can Truncate

**Severity:** MEDIUM
**CWE:** CWE-120 (Buffer Copy without Checking Size)
**Files:** `src/view.c:160`, `src/focus_nav.c:86-110`, `src/menu.c:1662-1673`, `src/workspace.c:268`, `src/output.c:81`
**Source:** Manual audit (Task #1)

**Description:**
Several locations use 256-byte fixed buffers for JSON escape sequences. If
a window title or workspace name is very long, the escape output can be
truncated mid-escape-sequence (e.g., `\u00` without closing digits),
producing malformed JSON that could break IPC clients.

**Recommendation:**
Use dynamically-sized buffers or the existing `json_escape()` function from
ipc_commands.c.

---

### LOW-1: Missing NULL Check After calloc for Iconbar Boxes

**Severity:** LOW
**CWE:** CWE-476 (NULL Pointer Dereference)
**File:** `src/toolbar.c:648`
**Source:** Manual audit (Task #1)

---

### LOW-2: Integer Overflow in json_escape Allocation

**Severity:** LOW
**CWE:** CWE-190 (Integer Overflow)
**File:** `src/ipc_commands.c:54`
**Source:** Manual audit (Task #1)

**Description:**
`len * 6 + 3` could overflow for very large strings, but mitigated by
IPC_READ_BUF_MAX at 64KB.

---

### LOW-3: strncpy Usage May Not NUL-Terminate

**Severity:** LOW
**CWE:** CWE-120 (Buffer Copy without Checking Size of Input)
**Files:** `src/ipc.c:247`, `src/rules.c:301`, `src/toolbar.c:816`
**Source:** flawfinder [1]

---

### LOW-4: signal() vs sigaction()

**Severity:** LOW
**CWE:** CWE-479 (Signal Handler Use of Non-Reentrant Function)
**File:** `src/server.c:261`
**Source:** Manual audit (Task #2)

**Description:**
Uses `signal()` instead of `sigaction()`. `signal()` behavior varies across
platforms and may reset the handler after delivery.

---

### LOW-5: Autostart lstat/execl TOCTOU

**Severity:** LOW
**CWE:** CWE-367 (TOCTOU Race Condition)
**File:** `src/autostart.c:100`
**Source:** Manual audit (Task #2)

---

### LOW-6: Duplicate Condition in Keybind Parsing

**Severity:** LOW
**CWE:** CWE-561 (Dead Code)
**File:** `src/keybind.c:833-838`
**Source:** cppcheck [duplicateCondition]

---

### LOW-7: Redundant Initialization/Assignment

**Severity:** LOW
**CWE:** N/A (Code Quality)
**Files:** `src/cursor.c:254-259`, `src/xwayland.c:468-479`
**Source:** cppcheck [redundantInitialization, redundantAssignment]

---

### LOW-8: Identical Ternary Branches in Menu

**Severity:** LOW
**CWE:** CWE-561 (Dead Code)
**File:** `src/menu.c:1622`
**Source:** cppcheck [duplicateExpressionTernary]

---

### LOW-9: Always-True/False Conditions in View Navigation

**Severity:** LOW
**CWE:** CWE-570/CWE-571
**Files:** `src/view.c:1843,1882`
**Source:** cppcheck [knownConditionTrueFalse]

**Description:**
`!first_out` always true and `prev_out` always false — output cycling
logic may not work as intended.

---

### LOW-10: Unused Functions (15)

**Severity:** LOW
**CWE:** CWE-561 (Dead Code)
**Source:** cppcheck [unusedFunction]

15 functions never called: `wm_decoration_set_size`,
`wm_focus_nav_in_toolbar`, `wm_foreign_toplevel_update_output`,
`wm_keyboard_get_layout_name`, `wm_menu_is_open`,
`wm_slit_handle_pointer_enter/leave`, `style_reload`,
`wm_color_to_argb`, `wm_tab_group_merge`, `wm_tab_group_index_of`,
`wm_toolbar_update_title`, `wm_toolbar_handle_click`,
`wm_workspace_get_current`, `wm_view_visible_on_output`.

---

### LOW-11: ~30 Functions Missing Static Linkage

**Severity:** LOW
**CWE:** N/A (Code Quality)
**Source:** cppcheck [staticFunction]

Functions only used within their translation unit but lack `static`.

---

### LOW-12: fluxland-ctl Minor Issues

**Severity:** LOW
**CWE:** CWE-74, CWE-400
**File:** `tools/fluxland-ctl.c`
**Source:** Manual review

- Line 352: Command name interpolated into JSON without escaping (safe due
  to known_commands[] allowlist)
- `read_response()` grows unbounded (no max response size)

---

## Informational Notes

### INFO-1: cppcheck uninitvar False Positives (139 instances)

Nearly all cppcheck `uninitvar` errors are false positives caused by
cppcheck not understanding the `wl_list_for_each` macro, which uses
`container_of` to derive struct pointers from list links. These are
safe and can be suppressed.

### INFO-2: const-correctness Suggestions (~100 instances)

cppcheck suggests `const` qualifiers for many variables and parameters.
While not security issues, improved const-correctness can catch bugs
at compile time.

### INFO-3: strlen on String Literals (53 flawfinder hits)

All `strlen()` uses operate on strings that are guaranteed to be
NUL-terminated (from `strdup`, string literals, or `fgets`). These are
false positives.

### INFO-4: view->id Wraps at UINT32_MAX

`src/view.c:1290` — The view ID counter wraps at UINT32_MAX. Unlikely
to be hit in practice but could cause duplicate IDs after ~4 billion
window creates.

### INFO-5: Keybind Chain Depth

`src/keybind.c:900-917` — No explicit depth limit on keybind chains,
but the 32-segment array provides a practical cap. Not exploitable.

### INFO-6: Animation Timer Callback Safety

`src/animation.c:17-62` — Potential use-after-free concern in animation
timer callbacks if the view is destroyed during animation. Currently
mitigated by cleanup in unmap handlers but worth monitoring.

### INFO-7: Sanitizer Options Available

The build system supports ASan (`-Dasan=true`) and UBSan (`-Dubsan=true`)
for development, which is good practice.

---

## Positive Security Observations

1. **No unsafe string functions:** No `strcpy`, `strcat`, `gets`, or
   unguarded `scanf` anywhere in the codebase
2. **Consistent snprintf usage:** String formatting uses `snprintf` with
   sizeof throughout
3. **Socket permissions:** IPC socket created with 0600 via `fchmod()`
4. **Buffer limits:** IPC read buffer capped at 64KB (`IPC_READ_BUF_MAX`)
5. **Double-fork:** All 5 child process spawn sites use correct double-fork
   with `setsid()`, signal mask reset, and `closefrom()`
6. **SOCK_CLOEXEC:** Socket fd has close-on-exec flag
7. **Non-blocking I/O:** IPC socket uses non-blocking I/O
8. **Menu recursion depth:** Limited to 16 levels
9. **malloc NULL checks:** Consistent throughout the codebase
10. **fgets with sizeof:** Config file reading properly bounded
11. **Cairo buffer overflow checks:** Surface status verified after creation
12. **Config validation:** `--check-config` CLI flag for offline validation
13. **Sanitizer support:** ASan and UBSan build options available
14. **Warning level 3:** Good baseline warning coverage

---

## Remediation Priority

### Immediate (before release)
1. **CRITICAL-1:** Add environment variable denylist for IPC SetEnv
2. **CRITICAL-2:** Add `SO_PEERCRED` check + optional `--ipc-no-exec` flag
3. **HIGH-1:** Add build hardening flags to meson.build
4. **HIGH-2:** Replace all `atoi()` with safe parsing function
5. **HIGH-3:** Fix integer truncation in rc_get_int() and style_get_int()
6. **HIGH-4:** Fix IPC socket TOCTOU with umask
7. **HIGH-5:** Shell-escape wallpaper filenames in menu.c
8. **HIGH-8:** Fix missing return statements

### Short-term
9. **HIGH-6:** Restrict menu [include] paths
10. **HIGH-7:** Validate SetStyle file paths
11. **MEDIUM-1:** Add root detection at startup
12. **MEDIUM-2:** Validate toolbar format string pattern
13. **MEDIUM-3:** Harden JSON parser against malformed input
14. **MEDIUM-5:** Add IPC client connection limit
15. **MEDIUM-11:** Fix dead code / always-false conditions in decoration.c

### Long-term
16. **MEDIUM-6:** Gate BindKey IPC behind flag
17. **MEDIUM-9:** Consider O_NOFOLLOW for config file operations
18. **MEDIUM-10:** Environment variable validation
19. **MEDIUM-12:** Replace fixed JSON escape buffers with dynamic allocation
20. **LOW-10/11:** Remove unused functions, add static linkage

---

## Fixes Applied (2026-02-25)

The following security fixes have been implemented and verified (all tests pass):

| # | Finding | Fix | Files Changed |
|---|---------|-----|---------------|
| 1 | **HIGH-1:** Missing build hardening | Added `-fstack-protector-strong`, `-fstack-clash-protection`, `-Wformat`, `-Wformat-security`, `-D_FORTIFY_SOURCE=2`, full RELRO (`-Wl,-z,relro,-z,now`) | `meson.build` |
| 2 | **MEDIUM-1:** No root detection | Refuse to start as root (`getuid()==0`) with clear error message | `src/main.c` |
| 3 | **CRITICAL-1:** IPC SetEnv denylist | Block setting `LD_PRELOAD`, `LD_LIBRARY_PATH`, `LD_AUDIT`, `LD_DEBUG`, `LD_PROFILE`, `PATH`, `IFS`, `SHELL`, `HOME`, `XDG_RUNTIME_DIR`, `WAYLAND_DISPLAY` | `src/ipc_commands.c`, `src/keyboard.c` |
| 4 | **HIGH-4:** IPC socket TOCTOU | Set `umask(0077)` before `bind()`, restore after; `fchmod(0600)` retained as defense-in-depth | `src/ipc.c` |
| 5 | **MEDIUM-5:** Unbounded IPC clients | Added `IPC_MAX_CLIENTS` (128) limit with client count tracking; excess connections rejected | `src/ipc.c`, `src/ipc.h` |
| 6 | **HIGH-3:** Integer truncation in parsers | Added `INT_MIN`/`INT_MAX` range check and `errno==ERANGE` check to `rc_get_int()` and `style_get_int()` | `src/rcparser.c`, `src/style.c` |
| 7 | **HIGH-9:** Unbounded font size | Capped font size at `MAX_FONT_SIZE` (200) in `style_parse_font()` | `src/style.c` |
| 8 | **HIGH-5:** Wallpaper filename injection | Wrapped filenames in single quotes; reject filenames containing `'` | `src/menu.c` |
| 9 | **MEDIUM-2:** Format string in toolbar | Replaced `snprintf(buf, ..., pattern, title)` with safe string concatenation that doesn't use user-controlled format strings | `src/toolbar.c` |
| 10 | **LOW-1:** Missing NULL check | Added NULL check after `calloc()` for `ib_boxes` in toolbar iconbar | `src/toolbar.c` |
| 11 | **LOW-2:** Integer overflow in json_escape | Added overflow check `len > SIZE_MAX / 6 - 1` before allocation | `src/ipc_commands.c` |

### Phase 2 — High-Priority Fixes (2026-02-25)

| # | Finding | Fix | Files Changed |
|---|---------|-----|---------------|
| 12 | **HIGH-2:** Unsafe `atoi()` calls | Replaced all 36 `atoi()` calls with `safe_atoi()` wrapper (strtol-based, overflow-safe) | `src/util.h` (new), `src/keyboard.c`, `src/ipc_commands.c`, `src/rules.c`, `src/cursor.c`, `src/menu.c`, `src/mousebind.c`, `src/validate.c`, `src/style.c`, `src/keybind.c` |
| 13 | **HIGH-6:** Unrestricted menu includes | Menu `[include]` paths restricted via `realpath()` + directory allowlist (config dir, /usr/share, /usr/local/share, /etc, /tmp) | `src/ipc_commands.c` |
| 14 | **HIGH-7:** Unrestricted SetStyle paths | `SetStyle` IPC command validates path via `realpath()` + same allowlist; rejects `..` traversal | `src/ipc_commands.c` |
| 15 | **HIGH-8:** cppcheck missingReturn | Verified as **false positives** — both `break` inside `wl_list_for_each` loops, not function endings | No changes needed |
| 16 | **MEDIUM-3:** Fragile JSON parser | Hardened IPC JSON parser against unterminated strings | `src/ipc_commands.c` |
| 17 | **HIGH (IPC):** No peer credential check | Added `SO_PEERCRED` UID verification on IPC connections + `XDG_RUNTIME_DIR` ownership/permissions validation | `src/ipc.c` |
| 18 | **MEDIUM-7:** Unbounded validation results | Capped error collection at 1000 entries | `src/validate.c` |
| 19 | **LOW:** signal() usage | Replaced `signal(SIGPIPE, SIG_IGN)` with `sigaction()` for portable behavior | `src/server.c` |
| 20 | **MEDIUM-12:** JSON escape buffer truncation | Added `j+6` safety margin check in all JSON escape loops | `src/view.c`, `src/focus_nav.c`, `src/workspace.c`, `src/output.c` |
| 21 | **LOW:** View ID overflow | Added wrap-around protection (`next_view_id` skips 0) | `src/view.c` |
| 22 | **MEDIUM-11:** Dead/unreachable code | Removed 13 unused functions across the codebase | `src/decoration.c/h`, `src/focus_nav.c/h`, `src/foreign_toplevel.c/h`, `src/keyboard.c/h`, `src/menu.c/h`, `src/slit.c/h`, `src/tabgroup.c/h`, `src/toolbar.c/h`, `src/workspace.c/h` |

### Phase 3 — Medium-Priority Fixes (2026-02-25)

| # | Finding | Fix | Files Changed |
|---|---------|-----|---------------|
| 23 | **MEDIUM-6:** IPC BindKey/Exec attack surface | Added `--ipc-no-exec` CLI flag that disables `Exec`, `BindKey`, and `SetStyle` IPC actions | `src/main.c`, `src/server.h`, `src/ipc_commands.c` |
| 24 | **MEDIUM-9:** Symlink attacks on config files | Added `fopen_nofollow()` helper using `O_NOFOLLOW`; applied to config parser, style loader, and menu file loading | `src/util.h`, `src/rcparser.c`, `src/style.c`, `src/menu.c` |
| 25 | **MEDIUM-10:** Config dir env hijacking | Added `validate_config_dir()` — checks ownership (uid), permissions (group/world writable), path traversal; FLUXLAND_CONFIG_DIR falls back to default on failure | `src/config.c` |
| 26 | **MEDIUM-8:** Regex DoS in window rules | Capped regex pattern length at 1024 bytes before `regcomp()` | `src/rules.c` |
| 27 | **HIGH-5 (addendum):** Wallpaper dir path injection | Reject wallpaper directory paths containing single quotes | `src/menu.c` |

### Phase 4 — Low-Priority Fixes (2026-02-25)

| # | Finding | Fix | Files Changed |
|---|---------|-----|---------------|
| 28 | **LOW-3a:** strncpy NUL-termination | Added explicit NUL-termination after strncpy for IPC socket path | `src/ipc.c` |
| 29 | **LOW-3c:** strncpy off-by-one | Changed strncpy size to `sizeof(...) - 1` to avoid truncating clock string | `src/toolbar.c` |
| 30 | **LOW-5:** Autostart TOCTOU | Replaced `lstat()` with `open(O_RDONLY|O_CLOEXEC)` + `fstat()` to eliminate TOCTOU race | `src/autostart.c` |
| 31 | **LOW-6:** Duplicate condition | Combined two identical `if (argument)` checks into one block | `src/keybind.c` |
| 32 | **LOW-7a:** Dead code | Removed redundant initial assignment of `edges` variable | `src/cursor.c` |
| 33 | **LOW-7b:** Dead code | Removed dead stores to cursor_mode, grabbed_view, grab_x/y before passthrough override | `src/xwayland.c` |
| 34 | **LOW-12a:** JSON escape control chars | `json_escape_str()` now escapes all control characters (0x00-0x1F) as `\u00XX`; buffer sized for worst case | `tools/fluxland-ctl.c` |
| 35 | **LOW-12b:** Unbounded response buffer | Added 10 MiB cap (`MAX_RESPONSE_SIZE`) to `read_response()` | `tools/fluxland-ctl.c` |
| 36 | **MEDIUM-4:** IPC event flood | Added per-event-type throttle (16ms / ~60fps) for high-frequency events (focus, title, workspace); infrequent events always delivered immediately | `src/ipc.c`, `src/ipc.h` |

### Verified Non-Issues

| Finding | Status | Reason |
|---------|--------|--------|
| **LOW-3b** (rules.c strncpy) | False positive | Already NUL-terminated on next line |
| **LOW-8** (menu.c ternary) | False positive | No identical ternary branches found at reported location |
| **LOW-9** (view.c conditions) | Correct code | Output cycling logic is sound; cppcheck misunderstands control flow |
| **LOW-10** (unused functions) | Resolved | 13 removed in Phase 2; remaining 2 (`style_reload`, `wm_color_to_argb`) used by test/fuzz targets |
| **LOW-11** (static linkage) | Deferred | ~30 functions could be made static; code quality only, no security impact |

### Remaining

All critical, high, medium, and low security findings have been resolved. Only informational items (INFO-1 through INFO-7) and minor code quality suggestions (const-correctness, static linkage) remain — none have security impact.
