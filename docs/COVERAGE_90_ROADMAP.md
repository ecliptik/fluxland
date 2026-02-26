# Fluxland Test Coverage Roadmap: 35% → 90%

## Current Status (Updated 2026-02-26)

**Current state:** 75.4% line coverage (11,643 / 15,442 executable lines) across 47 source modules
**Target:** 90% line coverage
**Realistic ceiling:** ~80-84% theoretical max
**Test files:** 35 (up from 15 at baseline)
**All tests passing:** 36/36

### Phases Completed

| Phase | Description | Result | Coverage |
|-------|-------------|--------|----------|
| Baseline | 15 test files | — | 35.0% |
| 1 | Quick Wins — extend tests + tabgroup/focus_nav | +3 files, ~122 tests | 40.5% |
| 2 | Mock Framework + Unit Tests | +8 files, ~150 tests | 48.6% |
| 3 | Protocol Integration Tests | +5 files, ~41 tests | 51.0% |
| 4 | Hard Modules (keyboard, cursor, xwayland, placement, render) | +3 files, ~132 tests | 56.7% |
| 5 | Polish (IPC server, systray, main CLI, autostart) | +3 files, ~49 tests | 57.7% |
| 5b | Deep push (rules, view, menu, decoration, workspace) | Extended 5 files | 61.9% |
| 6 | Deep Coverage Push (toolbar, ipc, output, slit, placement) | Extended + new files | 68.7% |
| 7 | Coverage push (keyboard, menu, cursor snap, more) | Extended 4 files | 70.9% |
| 8 | Final push (cursor, keyboard, menu, server, systray) | +1 new file, extended 4 | 75.4% |

### Key Decisions Made
- **No shared mock framework needed:** The `#include "source.c"` pattern with header guards
  scaled successfully to all modules including the hardest ones (keyboard.c at 2908 lines)
- **Keyboard refactor deferred:** Achieved 82.5% coverage via extensive stubs, deferring the
  vtable refactoring as a future TODO since diminishing returns
- **Protocol headers generated:** wayland-scanner client headers for layer-shell, session-lock,
  idle-inhibit, idle-notify via Meson custom_target()
- **Integration tests work:** Headless compositor fork + Wayland client pattern covers protocol code
- **D-Bus stubs:** Configurable sd_bus stubs with stdarg.h enabled testing systray D-Bus callbacks

---

## What Remains (Future Sessions)

### Keyboard Refactoring (TODO — deferred)

The highest single-module ROI remaining. Deferred as a future TODO — current keyboard.c
is at 82.5% via extensive stubs, and this refactor is a significant architectural change.

- **Refactor keyboard.c** into:
  - `keyboard_actions.c/h` — action dispatch via vtable (70 function pointers)
  - `condition_eval.c/h` — `evaluate_condition()`, `match_property()`, `bool_match()`
  - `chain_state.c/h` — chain timeout state machine
- This would make the remaining ~17% of keyboard.c (event handlers, key processing)
  testable via the vtable pattern instead of 65+ individual stubs
- **Estimated gain: keyboard.c 82.5% → 90%+, overall +1%**
- **Prerequisite:** Should be done when there's appetite for a larger refactor, not just test additions

### Remaining Module Targets (est. → 78-80%)

| Module | Current | Target | Approach | Effort |
|--------|---------|--------|----------|--------|
| protocols.c | 38.2% | 55% | Exercise more _create() paths via real clients | MEDIUM |
| session_lock.c | 73.8% | 85% | More lock/unlock lifecycle edge cases | EASY |
| main.c | 64.8% | 75% | CLI flag parsing edge cases | EASY |
| autostart.c | 53.7% | 70% | More startup file parsing paths | EASY |

**Estimated gain: +2-4 percentage points**

### XWayland Integration Tests (est. → 80-82%)

| Module | Current | Approach |
|--------|---------|----------|
| xwayland.c | 14.5% | XWayland surface lifecycle (requires XWayland in test env) |
| protocols.c | 38.2% | Exercise more protocol _create() paths via real clients |

**Estimated gain: +2-3 percentage points**

---

## What's Unreachable (Stays Low)

These modules or code paths realistically **cannot** reach high coverage:

| Module/Path | Reason | Realistic Max |
|-------------|--------|---------------|
| cursor.c event handlers | Remaining 60% is deeply coupled wlr_cursor/scene graph glue | ~40% |
| input.c (22.0%) | 100% wlroots event handler glue | 22% (integration only) |
| text_input.c (12.6%) | Needs simultaneous text-input-v3 + input-method-v2 clients | 15% |
| tablet.c (4.5%) | Requires tablet hardware simulation | 5% |
| drm_lease.c / drm_syncobj.c | Require DRM backend (not headless) | Current |
| gamma_control.c (21.9%) | Output commit fails on pixman renderer | Current |
| exec_command() paths | Real fork/exec in keyboard.c, ipc_commands.c, menu.c | Untestable |
| main.c server_run() | Full event loop | Integration only |
| screencopy.c / presentation.c / viewporter.c / fractional_scale.c | Pure boilerplate wlroots _create() calls | Current |

**Impact**: These ~2,500 lines cap the theoretical maximum at ~84% with unit + integration tests.

---

## Per-Module Coverage Summary (Current)

| Module | Lines | Current % | Status |
|--------|-------|-----------|--------|
| foreign_toplevel.c | 119 | 96.6% | DONE |
| placement.c | 394 | 93.1% | DONE |
| server.c | 285 | 92.6% | DONE |
| decoration.c | 953 | 92.2% | DONE |
| output.c | 115 | 89.6% | DONE |
| tabgroup.c | 153 | 89.5% | DONE |
| mousebind.c | 296 | 87.8% | DONE |
| validate.c | 409 | 87.3% | DONE |
| systray.c | 364 | 87.1% | DONE |
| util.h | 23 | 87.0% | DONE |
| animation.c | 71 | 85.9% | DONE |
| config.c | 628 | 84.6% | DONE |
| output_management.c | 108 | 84.3% | DONE |
| rules.c | 486 | 83.3% | DONE |
| style.c | 582 | 83.7% | DONE |
| layer_shell.c | 206 | 83.0% | DONE |
| keybind.c | 561 | 82.9% | DONE |
| ipc_commands.c | 984 | 82.6% | DONE |
| keyboard.c | 1064 | 82.5% | DONE (refactor TODO) |
| focus_nav.c | 139 | 82.0% | DONE |
| workspace.c | 274 | 81.8% | DONE |
| toolbar.c | 850 | 81.5% | DONE |
| slit.c | 506 | 80.8% | DONE |
| render.c | 421 | 80.8% | DONE |
| view.c | 937 | 80.4% | DONE |
| menu.c | 1554 | 80.0% | DONE |
| rcparser.c | 110 | 80.0% | DONE |
| session_lock.c | 168 | 73.8% | DONE |
| fractional_scale.c | 7 | 71.4% | SKIP |
| ipc.c | 222 | 70.7% | DONE |
| idle.c | 65 | 70.8% | DONE |
| main.c | 71 | 64.8% | DONE |
| style.h | 8 | 100% | DONE |
| screencopy.c | 11 | 63.6% | SKIP |
| viewporter.c | 11 | 63.6% | SKIP |
| presentation.c | 6 | 66.7% | SKIP |
| drm_syncobj.c | 13 | 61.5% | SKIP |
| autostart.c | 54 | 53.7% | DONE |
| transient_seat.c | 25 | 48.0% | SKIP |
| drm_lease.c | 26 | 42.3% | SKIP |
| cursor.c | 953 | 39.5% | DONE |
| protocols.c | 249 | 38.2% | Future |
| input.c | 100 | 22.0% | SKIP |
| gamma_control.c | 32 | 21.9% | SKIP |
| xwayland.c | 392 | 14.5% | Future |
| text_input.c | 214 | 12.6% | SKIP |
| tablet.c | 223 | 4.5% | SKIP |

---

## Test Infrastructure Summary

### Test Pattern
All tests use the `#include "source.c"` pattern:
- Header guards block real wlroots/wayland/system headers
- Stub types defined locally in each test file
- Stub functions with call counters for dispatch verification
- wlroots linker symbol override (dynamically linked `libwlroots-0.18.so`)
- No shared mock framework needed — each test is self-contained

### Integration Test Harness
- `tests/integration/harness.c/h` — forks compositor with `WLR_BACKENDS=headless WLR_RENDERER=pixman`
- Protocol headers generated via wayland-scanner custom_target() in Meson
- Covers: xdg-shell, layer-shell, session-lock, idle-inhibit, idle-notify

### Visual Regression Tests
- `tests/visual/image_compare.c/h` — Cairo-based pixel comparison
- `tests/visual/test_decorations.c` — gradient types, text rendering, bevel styles

### Coverage Measurement
```bash
meson setup buildcov -Db_coverage=true
ninja -C buildcov
meson test -C buildcov --print-errorlogs
lcov --capture --directory buildcov --output-file buildcov/coverage.info \
  --exclude '*/tests/*' --exclude '/usr/*' --ignore-errors unused
lcov --list buildcov/coverage.info --ignore-errors unused
```
