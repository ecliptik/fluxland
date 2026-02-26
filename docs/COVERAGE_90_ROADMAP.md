# Fluxland Test Coverage Roadmap: 35% → 90%

## Current Status (Updated 2026-02-26)

**Current state:** 57.7% line coverage (8,908 / 15,442 executable lines) across 46 source modules
**Target:** 90% line coverage
**Realistic ceiling:** ~75-80% (unit + integration), ~84% theoretical max
**Test files:** 33 (up from 15 at baseline)
**All tests passing:** 33/33

### Phases Completed

| Phase | Description | Result | Coverage |
|-------|-------------|--------|----------|
| Baseline | 15 test files | — | 35.0% |
| 1 | Quick Wins — extend tests + tabgroup/focus_nav | +3 files, ~122 tests | 40.5% |
| 2 | Mock Framework + Unit Tests | +8 files, ~150 tests | 48.6% |
| 3 | Protocol Integration Tests | +5 files, ~41 tests | 51.0% |
| 4 | Hard Modules (keyboard, cursor, xwayland, placement, render) | +3 files, ~132 tests | 56.7% |
| 5 | Polish (IPC server, systray, main CLI, autostart) | +3 files, ~49 tests | 57.7% |
| 5b | Coverage push (rules, view, menu, decoration, workspace) | In progress | ~65% est |

### Key Decisions Made
- **No shared mock framework needed:** The `#include "source.c"` pattern with header guards
  scaled successfully to all modules including the hardest ones (keyboard.c at 2908 lines)
- **No keyboard.c refactoring needed:** Achieved 50.1% coverage with extensive stubs instead
  of the originally proposed vtable refactoring
- **Protocol headers generated:** wayland-scanner client headers for layer-shell, session-lock,
  idle-inhibit, idle-notify via Meson custom_target()
- **Integration tests work:** Headless compositor fork + Wayland client pattern covers protocol code

---

## What Remains (Future Sessions)

### Phase 6: Deep Coverage Push (est. → 70-75%)

These modules have testable code paths remaining but need careful stub work:

| Module | Current | Target | Approach | Effort |
|--------|---------|--------|----------|--------|
| toolbar.c | 72.6% | 85% | Extend test_toolbar_layout.c: more layout edge cases, iconbar update | MEDIUM |
| ipc.c | 60.4% | 80% | Extend test_ipc_server.c: socket accept, full lifecycle | MEDIUM |
| server.c | 59.3% | 75% | Extend integration tests: reconfigure, destroy paths | MEDIUM |
| output.c | 65.2% | 80% | NEW test_output.c: output add/remove, mode selection | MEDIUM |
| output_management.c | 39.8% | 60% | NEW test_output_mgmt.c: config apply/test paths | MEDIUM |
| slit.c | 58.7% | 70% | Extend test_slit.c: more layout edge cases | EASY |
| placement.c | 53.3% | 70% | Extend test_placement.c: more strategy edge cases | EASY |

**Estimated gain: +7-10 percentage points**

### Phase 7: Keyboard Refactoring (TODO — deferred)

The highest single-module ROI remaining. Deferred as a future TODO — current keyboard.c
is at 64.8% via extensive stubs, and this refactor is a significant architectural change.

- **Refactor keyboard.c** into:
  - `keyboard_actions.c/h` — action dispatch via vtable (70 function pointers)
  - `condition_eval.c/h` — `evaluate_condition()`, `match_property()`, `bool_match()`
  - `chain_state.c/h` — chain timeout state machine
- This would make the remaining ~35% of keyboard.c (event handlers, key processing)
  testable via the vtable pattern instead of 65+ individual stubs
- **Estimated gain: keyboard.c 64.8% → 80%+, overall +2-3%**
- **Prerequisite:** Should be done when there's appetite for a larger refactor, not just test additions

### Phase 8: Advanced Integration Tests (est. → 80-84%)

Deeper integration test coverage for protocol handlers:

| Module | Current | Approach |
|--------|---------|----------|
| cursor.c | 16.5% | Multi-client move/resize integration tests |
| xwayland.c | 14.5% | XWayland surface lifecycle integration (requires XWayland in test) |
| output_management.c | 39.8% | Full config apply/test cycle via protocol |
| protocols.c | 38.2% | Exercise more protocol _create() paths via real clients |

**Estimated gain: +5-8 percentage points**

---

## What's Unreachable (Stays Low)

These modules or code paths realistically **cannot** reach high coverage:

| Module/Path | Reason | Realistic Max |
|-------------|--------|---------------|
| cursor.c event handlers | Deeply coupled to wlr_cursor, scene graph, touch/gesture protocols | 30% |
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
| tabgroup.c | 153 | 89.5% | DONE |
| mousebind.c | 296 | 87.8% | DONE |
| validate.c | 409 | 87.3% | DONE |
| animation.c | 71 | 85.9% | DONE |
| config.c | 628 | 84.6% | DONE |
| style.c | 582 | 83.7% | DONE |
| layer_shell.c | 206 | 83.0% | DONE |
| keybind.c | 561 | 82.9% | DONE |
| ipc_commands.c | 984 | 82.6% | DONE |
| focus_nav.c | 139 | 82.0% | DONE |
| render.c | 421 | 80.8% | DONE |
| rcparser.c | 110 | 80.0% | DONE |
| session_lock.c | 168 | 73.8% | DONE |
| toolbar.c | 850 | 72.6% | Phase 6 |
| idle.c | 65 | 70.8% | DONE |
| main.c | 71 | 64.8% | DONE |
| output.c | 115 | 65.2% | Phase 6 |
| rules.c | 486 | 62.3% | Phase 5b (in progress) |
| ipc.c | 222 | 60.4% | Phase 6 |
| server.c | 285 | 59.3% | Phase 6 |
| slit.c | 506 | 58.7% | Phase 6 |
| decoration.c | 953 | 57.4% | Phase 5b (in progress) |
| autostart.c | 54 | 53.7% | DONE |
| placement.c | 394 | 53.3% | Phase 6 |
| workspace.c | 274 | 51.1% | Phase 5b (in progress) |
| keyboard.c | 1064 | 50.1% | Phase 7 (refactor) |
| menu.c | 1554 | 44.7% | Phase 5b (in progress) |
| view.c | 937 | 40.8% | Phase 5b (in progress) |
| output_management.c | 108 | 39.8% | Phase 6 |
| protocols.c | 249 | 38.2% | Phase 8 |
| systray.c | 364 | 33.8% | DONE |
| gamma_control.c | 32 | 21.9% | SKIP |
| input.c | 100 | 22.0% | SKIP |
| cursor.c | 953 | 16.5% | Phase 8 |
| xwayland.c | 392 | 14.5% | Phase 8 |
| text_input.c | 214 | 12.6% | SKIP |
| tablet.c | 223 | 4.5% | SKIP |
| drm_lease.c | 26 | 42.3% | SKIP |
| drm_syncobj.c | 13 | 61.5% | SKIP |
| fractional_scale.c | 7 | 71.4% | SKIP |
| presentation.c | 6 | 66.7% | SKIP |
| screencopy.c | 11 | 63.6% | SKIP |
| transient_seat.c | 25 | 48.0% | SKIP |
| viewporter.c | 11 | 63.6% | SKIP |
| style.h | 8 | 100% | DONE |
| util.h | 23 | 82.6% | DONE |

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
