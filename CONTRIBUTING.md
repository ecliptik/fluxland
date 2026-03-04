# Contributing to fluxland

Thank you for your interest in contributing to fluxland! This document
covers the development workflow, project architecture, and how to add
common types of features.

## License

This project is licensed under the MIT License. By contributing, you agree
that your contributions will be licensed under the same terms.

## Prerequisites

Install the following development dependencies:

**Debian/Ubuntu:**
```sh
sudo apt install meson ninja-build pkg-config libwlroots-0.18-dev \
  libwayland-dev wayland-protocols libxkbcommon-dev libinput-dev \
  libpixman-1-dev libpango1.0-dev libdrm-dev
```

**Optional (XWayland support):**
```sh
sudo apt install xwayland libxcb1-dev libxcb-ewmh-dev libxcb-icccm4-dev
```

## Building for development

### Debug build (default)

```sh
meson setup build --buildtype=debug
ninja -C build
```

### With sanitizers

Address sanitizer (memory errors):
```sh
meson setup build -Dasan=true
ninja -C build
```

Undefined behavior sanitizer:
```sh
meson setup build -Dubsan=true
ninja -C build
```

Both:
```sh
meson setup build -Dasan=true -Dubsan=true
ninja -C build
```

### Running

```sh
# In a TTY (DRM backend):
./build/fluxland

# Nested in another Wayland compositor (for development):
WLR_BACKENDS=wayland ./build/fluxland

# Headless (for testing):
WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/fluxland
```

### Running tests

```sh
# C unit tests (53 tests)
meson test -C build --print-errorlogs

# Python UI tests (174 tests, requires running compositor)
python3 -m pytest tests/ui/ --timeout=30
```

### Validating configuration

Use the `--check-config` flag to validate all configuration files
without starting the compositor:

```sh
./build/fluxland --check-config
```

This checks the init, keys, and style files for syntax errors,
unknown keys, and invalid values.

## Code style

- **Language:** C17 (`-std=c17`)
- **Indentation:** Tabs (width 8)
- **Column limit:** 80 characters
- **Brace style:** Linux (K&R for functions)
- **Pointer alignment:** Right (`char *p`, not `char* p`)
- **Return type:** Top-level definitions break after return type

A `.clang-format` file is provided. Format with:
```sh
clang-format -i src/yourfile.c
```

General conventions:
- Follow existing patterns in the codebase
- Use `wm_` prefix for all public symbols
- Use `static` for file-local functions
- Use `wlr_log()` for logging (WLR_DEBUG, WLR_INFO, WLR_ERROR)
- Guard headers with `#ifndef WM_MODULENAME_H`
- Use `_POSIX_C_SOURCE 200809L` in .c files that need POSIX APIs

## Project architecture

### Lifecycle (server.c)

1. `wm_server_init()` - Creates wl_display, wlroots backend, renderer,
   scene graph, seat, and initializes all protocol handlers
2. `wm_server_start()` - Starts the backend, binds the Wayland socket
3. `wm_server_run()` - Enters the wl_display event loop
4. `wm_server_destroy()` - Tears down everything in reverse order

### Scene graph

The compositor uses wlroots' scene graph API (`wlr_scene`) for rendering.
Views are organized into layer trees within `wm_server`:

```
scene
  +-- layer_background   (layer-shell background)
  +-- layer_bottom        (layer-shell bottom)
  +-- view_tree
  |     +-- view_layer_desktop
  |     +-- view_layer_below
  |     +-- view_layer_normal   (most windows live here)
  |     +-- view_layer_above
  +-- xdg_popup_tree
  +-- layer_top           (layer-shell top: panels, bars)
  +-- layer_overlay        (layer-shell overlay: lock screens)
```

### Module pattern

Each feature is isolated into a header/source pair (e.g., `idle.h`/`idle.c`).
Modules follow a consistent pattern:

- An `_init()` function that registers Wayland protocol handlers
- A `_finish()` function that cleans up listeners
- State stored either in `struct wm_server` or a dedicated struct

### Key modules

| Module | Purpose |
|--------|---------|
| `server.c` | Core compositor lifecycle and initialization |
| `view.c` | XDG toplevel window management |
| `output.c` | Output (monitor) handling and frame rendering |
| `keyboard.c` | Keyboard input and action dispatch |
| `cursor.c` | Pointer input, interactive move/resize |
| `workspace.c` | Virtual desktop management |
| `config.c` | Configuration file loading (Fluxbox init format) |
| `keybind.c` | Keybinding parser (Fluxbox keys format) |
| `decoration.c` | Server-side window decorations |
| `protocols.c` | Misc Wayland protocol implementations |
| `ipc.c` / `ipc_commands.c` | Unix socket IPC server |

## How to add a new keybinding action

Adding a new action requires changes to **4 files**:

### 1. `src/keybind.h` - Define the enum value

Add your action to `enum wm_action`:

```c
enum wm_action {
    ...
    WM_ACTION_MY_ACTION,
};
```

### 2. `src/keybind.c` - Register the action name

Add an entry to the `actions[]` table (keep alphabetical order):

```c
static const struct action_map actions[] = {
    ...
    {"MyAction", WM_ACTION_MY_ACTION},
    ...
};
```

This maps the string name used in keys files (e.g., `:MyAction`) to the
enum value. The lookup is case-insensitive.

### 3. `src/keyboard_actions.c` - Implement the action

Add a `case` to the `wm_execute_action()` switch statement:

```c
case WM_ACTION_MY_ACTION:
    if (view) {
        /* your implementation here */
    }
    return true;
```

### 4. `src/ipc_commands.c` - Add IPC support

Add the action to `action_table[]` and add a matching `case` in
`ipc_execute_action()`. Both dispatch functions should share the same
logic:

```c
// In action_table[]:
{"MyAction", WM_ACTION_MY_ACTION},

// In ipc_execute_action():
case WM_ACTION_MY_ACTION:
    if (view) {
        /* same implementation as keyboard_actions.c */
    }
    return true;
```

## How to add a new Wayland protocol

### 1. `src/server.h` - Add state to the server struct

Add forward declarations and fields to `struct wm_server`:

```c
struct wlr_my_protocol_manager_v1;

struct wm_server {
    ...
    struct wlr_my_protocol_manager_v1 *my_protocol_mgr;
    struct wl_listener my_protocol_event;
};
```

### 2. Implement the protocol

For small protocols, add to `src/protocols.c`. For larger ones, create a
new `src/myprotocol.h` + `src/myprotocol.c` pair.

```c
static void
handle_my_protocol_event(struct wl_listener *listener, void *data)
{
    struct wm_server *server = wl_container_of(listener, server,
        my_protocol_event);
    /* handle the event */
}
```

Initialize in `wm_protocols_init()` (or your module's init function):

```c
server->my_protocol_mgr =
    wlr_my_protocol_manager_v1_create(server->wl_display);

server->my_protocol_event.notify = handle_my_protocol_event;
wl_signal_add(&server->my_protocol_mgr->events.some_event,
    &server->my_protocol_event);
```

### 3. Wire into the build

If you created new source files, add them to `wm_sources` in `meson.build`:

```meson
wm_sources = files(
    ...
    'src/myprotocol.c',
)
```

### 4. Clean up

Add listener cleanup in `wm_protocols_finish()` or your module's finish
function:

```c
wl_list_remove(&server->my_protocol_event.link);
```

## How to add a new config option

### 1. `src/config.h` - Add the field

```c
struct wm_config {
    ...
    int my_option;
};
```

### 2. `src/config.c` - Set default and parse

Set the default in `config_create()`, then handle the key in the parsing
logic (uses X resource database format: `session.MyOption: value`):

```c
// In config_create():
config->my_option = 42;  /* default */

// In the parsing section of config_load():
if (strcasecmp(key, "session.MyOption") == 0) {
    safe_atoi(value, &config->my_option);
}
```

### 3. Wire in the consumer

Use `server->config->my_option` wherever the option is needed. If the
option should take effect on reconfigure (SIGHUP / `:Reconfigure`),
read it in `wm_server_reconfigure()`.

## How to write a fuzz target

Fluxland includes 7 fuzz targets in `tests/fuzz/` using libFuzzer.
Fuzz targets require Clang to build.

### Existing targets

| Target | File | What it fuzzes |
|--------|------|---------------|
| `fuzz_rcparser` | `tests/fuzz/fuzz_rcparser.c` | X resource database parser |
| `fuzz_keybind` | `tests/fuzz/fuzz_keybind.c` | Key binding parser |
| `fuzz_style` | `tests/fuzz/fuzz_style.c` | Style parser |
| `fuzz_menu` | `tests/fuzz/fuzz_menu.c` | Menu file parser |
| `fuzz_ipc_command` | `tests/fuzz/fuzz_ipc_command.c` | IPC command parser |
| `fuzz_rules` | `tests/fuzz/fuzz_rules.c` | Window rules parser |
| `fuzz_mousebind` | `tests/fuzz/fuzz_mousebind.c` | Mouse binding parser |

### Adding a new fuzz target

1. Create `tests/fuzz/fuzz_myparser.c` with a `LLVMFuzzerTestOneInput`
   entry point:

```c
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Feed data to your parser */
    return 0;
}
```

2. Add the target to `tests/meson.build` in the fuzz targets section.

3. Build with Clang and run:

```sh
CC=clang meson setup build-fuzz -Db_sanitize=address,fuzzer
ninja -C build-fuzz
./build-fuzz/fuzz_myparser
```

## WLCS integration testing

WLCS (Wayland Conformance Test Suite) integration is available via
`src/wlcs_shim.c`. This is a Google Test-based test suite that
validates Wayland protocol conformance.

To build and run WLCS tests:

```sh
meson setup build_wlcs -Dwlcs=enabled
ninja -C build_wlcs
# Run via the WLCS test runner
```

## Commit message convention

Follow the [Conventional Commits](https://www.conventionalcommits.org/)
format:

```
<type>: <short description>

[optional body]
```

Types:
- `feat:` - New feature or capability
- `fix:` - Bug fix
- `docs:` - Documentation changes
- `test:` - Adding or updating tests
- `build:` - Build system or dependency changes
- `refactor:` - Code restructuring without behavior change
- `style:` - Formatting, whitespace (no code change)
- `chore:` - Maintenance tasks

Examples:
```
feat: add quarter-tiling snap zones
fix: prevent crash when output is removed during resize
docs: update keybinding reference in man page
test: add config parser edge case tests
build: add meson option for LTO
```

## Pull request process

1. Fork the repository and create a feature branch from `main`
2. Make your changes following the code style and conventions above
3. Ensure the project builds cleanly with `-Dwarning_level=3`
4. Run `meson test -C build` and confirm all tests pass
5. Write a clear PR description explaining what and why
6. One approval is required before merging
7. Squash-merge is preferred for single-feature branches
