# Fluxland Test Coverage Roadmap: 35% → 90%

## Executive Summary

**Current state:** 35.0% line coverage (5,387 / 15,401 executable lines) across 45 source modules
**Target:** 90% line coverage (13,860 lines)
**Gap:** 8,473 additional lines to cover
**Feasibility:** Achievable with a phased approach — 71.5% of the gap is in EASY/MEDIUM modules testable with existing patterns

### Key Numbers
- **15 test files** exist today (11 original + 4 added in QA sprint)
- **~25 new test files** needed to reach 90%
- **~400 new test cases** estimated
- **1 shared mock framework** required (tests/mocks/)
- **1 source refactor** recommended (keyboard.c action dispatch)

---

## Infrastructure Required

### 1. Shared Mock Framework (`tests/mocks/`)

The `#include "source.c"` pattern from test_animation.c doesn't scale beyond simple modules.
Create a shared mock directory:

```
tests/mocks/
├── wayland_mocks.h      — wl_list, wl_listener, wl_event_source, wl_signal stubs
├── wlroots_mocks.h      — ~25 stub types + ~60 no-op function declarations
├── xkb_mocks.h          — xkbcommon type/function stubs
├── server_mock.h        — Minimal wm_server struct (only fields tests access)
├── view_mock.h          — Minimal wm_view, wm_workspace, wm_decoration
└── fluxland_stubs.c     — Default no-op implementations of wm_* cross-module functions
```

**Key types to stub (~25):** wlr_scene, wlr_scene_tree, wlr_scene_node, wlr_scene_buffer,
wlr_keyboard, wlr_cursor, wlr_seat, wlr_xdg_toplevel, wlr_xdg_surface, wlr_surface,
wlr_output, wlr_output_layout, wlr_buffer, wlr_session, wlr_backend, wlr_renderer

**Critical discovery:** wlroots is dynamically linked (`libwlroots-0.18.so`), so tests can
override any wlr_* function by defining a stub in the test .c file — the linker prefers
object-file symbols over shared library symbols. This eliminates the need for complex
mock injection.

### 2. Integration Harness Improvements

The existing harness (tests/integration/harness.c) needs:
- **Protocol header generation** for layer-shell, foreign-toplevel, session-lock, idle-inhibit
  (wayland-scanner already used for xdg-shell — duplicate the pattern)
- **Multi-client support** (second `wl_display_connect()` for foreign-toplevel tests)
- **Roundtrip timeout** to prevent test hangs

### 3. Keyboard.c Refactoring (Optional but High-ROI)

Extract from keyboard.c's monolithic `execute_action()` (934-line switch):
- `keyboard_actions.c/h` — action dispatch via vtable (70 function pointers)
- `condition_eval.c/h` — `evaluate_condition()`, `match_property()`, `bool_match()`
- `chain_state.c/h` — chain timeout state machine

This makes 70 switch cases testable with a mock vtable instead of 65+ individual stubs.
Without refactoring, keyboard.c can still reach ~50% via the shared mock framework.

---

## Implementation Phases

### Phase 1: Quick Wins — Extend Existing Tests (est. +8% → 43%)
*Modules testable TODAY with zero new infrastructure*

| Module | Current | Target | Approach | New Tests |
|--------|---------|--------|----------|-----------|
| keybind.c | 43.7% | 90% | Extend test_keybind.c: mouse bindings, invalid input, keybind_find NULL | ~15 |
| rules.c | 52.9% | 90% | Extend test_rules.c: invalid regex, match logic, missing [end] | ~12 |
| config.c | 78.8% | 92% | Extend test_config.c: malformed lines, duplicate keys, slit/toolbar variants | ~10 |
| style.c | 78.9% | 92% | Extend test_style.c: invalid formats, edge values, menu textures | ~8 |
| validate.c | 81.7% | 92% | Extend test_validate.c: texture/font validation, multiple errors | ~6 |
| rcparser.c | 71.8% | 92% | Extend test_config.c: boundary parsing, UTF-8 | ~4 |
| ipc_commands.c | 24.4% | 48% | Extend test_ipc.c (Group A+B): 30+ simple action dispatch + mock view | ~40 |
| tabgroup.c | 0% | 85% | NEW test_tabgroup.c: pure state machine, all 8 public functions | ~15 |
| focus_nav.c | 5.0% | 77% | NEW test_focus_nav.c: state machine transitions, all 7 functions | ~12 |

**Subtotal: ~122 new tests, +1,235 lines covered**

### Phase 2: Shared Mock Framework + Unit Tests (est. +20% → 63%)
*Requires tests/mocks/ infrastructure*

| Module | Current | Target | Approach | New Tests |
|--------|---------|--------|----------|-----------|
| ipc_commands.c | 48% | 85% | Add 7 wlroots stubs + mock structs (Groups C+D+E) | ~50 |
| menu.c | 19.8% | 46% | NEW test_menu_interaction.c: key nav, hit testing, type-ahead, window menu creation | ~25 |
| decoration.c | 40.9% | 65% | NEW test_decoration_logic.c: button_at, region_at, tab_at, get_extents, button layout | ~18 |
| workspace.c | 17.5% | 60% | NEW test_workspace.c: index arithmetic, json_escape, get/get_active | ~15 |
| view.c | 32.8% | 55% | NEW test_view_logic.c: focus cycling, json_escape, layer_from_name, geometry | ~15 |
| toolbar.c | 62.1% | 75% | NEW test_toolbar_layout.c: parse_tool_name, compute_layout, iconbar filtering | ~12 |
| slit.c | 21.9% | 40% | NEW test_slit.c: compute_position, layout_clients, slitlist I/O | ~10 |
| server.c | 59.3% | 75% | Extend integration tests: server init validation | ~5 |

**Subtotal: ~150 new tests, +3,087 lines covered**

### Phase 3: Integration Tests for Protocols (est. +8% → 71%)
*Requires harness protocol header generation*

| Module | Current | Target | Approach | New Tests |
|--------|---------|--------|----------|-----------|
| layer_shell.c | 17.5% | 65% | NEW test_layer_shell.c: create surface on each layer, exclusive zones, keyboard interactivity | ~12 |
| foreign_toplevel.c | 54.6% | 80% | NEW test_foreign_toplevel.c: subscribe events, activate/max/min/fullscreen/close requests | ~10 |
| session_lock.c | 14.3% | 65% | NEW test_session_lock.c: lock lifecycle, lock surface configure, double-lock rejection | ~8 |
| idle.c | 32.3% | 70% | NEW test_idle.c: inhibitor create/destroy, state management | ~6 |
| output_management.c | 39.8% | 60% | NEW test_output_mgmt.c: subscribe config, test/apply paths | ~5 |

**Subtotal: ~41 new tests, +715 lines covered**

### Phase 4: Hard Modules — Refactoring + Deep Mocks (est. +12% → 83%)
*Requires keyboard.c refactoring or extensive mock framework*

| Module | Current | Target | Approach | New Tests |
|--------|---------|--------|----------|-----------|
| keyboard.c | 0% | 55% | Refactor execute_action → vtable; test conditions, chains, security, 70 action cases | ~40 |
| cursor.c | 6.1% | 30% | NEW test_cursor_snap.c: snap zone detection/geometry (pure math), mouse context | ~12 |
| xwayland.c | 3.8% | 25% | NEW test_xwayland_logic.c: classify_window, should_dock_in_slit (with mock structs) | ~8 |
| render.c | 68.6% | 85% | Extend test_decorations.c: more gradient types, text justify, HiDPI | ~8 |
| placement.c | 14.0% | 70% | REWRITE test_placement.c: test actual placement.c functions, not reimplemented math | ~10 |

**Subtotal: ~78 new tests, +1,920 lines covered**

### Phase 5: Remaining Coverage + Polish (est. +7% → 90%)
*Mop-up to reach 90% target*

| Module | Current | Target | Approach | New Tests |
|--------|---------|--------|----------|-----------|
| ipc.c | 32.0% | 80% | NEW test_ipc_server.c: socket handling, buffer ops, event broadcast | ~12 |
| autostart.c | 53.7% | 90% | Extend test_autostart.c: more edge cases | ~5 |
| systray.c | 18.7% | 30% | NEW test_systray.c: layout math, get_width, handle_click (if WM_HAS_SYSTRAY) | ~6 |
| main.c | 36.6% | 60% | Integration smoke: --version, --check-config, --list-commands exit codes | ~4 |
| input.c | 22.0% | 22% | SKIP — pure wlroots glue, coverage comes from integration tests | 0 |
| text_input.c | 12.6% | 15% | DEFER — needs dual-client IME relay mock | 0 |
| menu.c visual | 46% | 55% | Extend test_decorations.c: menu title/item rendering | ~5 |

**Subtotal: ~32 new tests, +516 lines covered**

---

## What's Unreachable (Won't Hit 90%)

These modules or code paths realistically **cannot** reach 90% coverage:

| Module/Path | Reason | Realistic Max |
|-------------|--------|---------------|
| cursor.c event handlers | Deeply coupled to wlr_cursor, scene graph, touch/gesture protocols | 30% |
| input.c | 100% wlroots event handler glue | 22% (integration only) |
| text_input.c | Needs simultaneous text-input-v3 + input-method-v2 clients | 15% |
| tablet.c | Requires tablet hardware simulation | 5% |
| drm_lease.c / drm_syncobj.c | Require DRM backend (not headless) | Current |
| gamma_control.c | Output commit fails on pixman renderer | Current |
| exec_command() (in keyboard.c, ipc_commands.c) | Real fork/exec | Untestable |
| main.c server_run() | Full event loop | Integration only |
| screencopy.c / presentation.c / viewporter.c / fractional_scale.c | Pure boilerplate wlroots _create() calls | Current |

**Impact**: These ~2,500 lines cap the theoretical maximum at ~84% if only unit-tested.
Integration tests (Phase 3) recover ~5-6% by exercising protocol handlers through the headless compositor.
The combined approach (unit + integration + visual) should reach **88-91%** depending on how well the headless backend exercises protocol code paths.

---

## Per-Module Coverage Summary

| Module | Lines | Current % | Phase | Target % | Tests Needed |
|--------|-------|-----------|-------|----------|-------------|
| animation.c | 71 | 85.9% | — | 90% | ~2 edge cases |
| mousebind.c | 296 | 87.8% | — | 92% | ~3 edge cases |
| config.c | 628 | 78.8% | 1 | 92% | ~10 |
| style.c | 582 | 78.9% | 1 | 92% | ~8 |
| validate.c | 409 | 81.7% | 1 | 92% | ~6 |
| rcparser.c | 110 | 71.8% | 1 | 92% | ~4 |
| keybind.c | 561 | 43.7% | 1 | 90% | ~15 |
| rules.c | 486 | 52.9% | 1 | 90% | ~12 |
| tabgroup.c | 153 | 0% | 1 | 85% | ~15 |
| focus_nav.c | 139 | 5.0% | 1 | 77% | ~12 |
| ipc_commands.c | 984 | 24.4% | 1+2 | 85% | ~90 |
| menu.c | 1554 | 19.8% | 2+5 | 55% | ~30 |
| decoration.c | 953 | 40.9% | 2 | 65% | ~18 |
| workspace.c | 274 | 17.5% | 2 | 60% | ~15 |
| view.c | 937 | 32.8% | 2 | 55% | ~15 |
| toolbar.c | 850 | 62.1% | 2 | 75% | ~12 |
| slit.c | 506 | 21.9% | 2 | 40% | ~10 |
| server.c | 285 | 59.3% | 2 | 75% | ~5 |
| layer_shell.c | 206 | 17.5% | 3 | 65% | ~12 |
| foreign_toplevel.c | 119 | 54.6% | 3 | 80% | ~10 |
| session_lock.c | 168 | 14.3% | 3 | 65% | ~8 |
| idle.c | 65 | 32.3% | 3 | 70% | ~6 |
| output_management.c | 108 | 39.8% | 3 | 60% | ~5 |
| keyboard.c | 1064 | 0% | 4 | 55% | ~40 |
| cursor.c | 944 | 6.1% | 4 | 30% | ~12 |
| xwayland.c | 391 | 3.8% | 4 | 25% | ~8 |
| render.c | 421 | 68.6% | 4 | 85% | ~8 |
| placement.c | 394 | 14.0% | 4 | 70% | ~10 |
| ipc.c | 222 | 32.0% | 5 | 80% | ~12 |
| autostart.c | 54 | 53.7% | 5 | 90% | ~5 |
| systray.c | 364 | 18.7% | 5 | 30% | ~6 |
| main.c | 71 | 36.6% | 5 | 60% | ~4 |
| input.c | 100 | 22.0% | — | 22% | SKIP |
| text_input.c | 214 | 12.6% | — | 15% | DEFER |
| tablet.c | 223 | 4.5% | — | 5% | SKIP |
| output.c | 115 | 65.2% | 2 | 80% | ~5 |
| protocols.c | 249 | 38.2% | — | 40% | Integration only |
| Tiny wrappers (7) | 99 | ~55% | — | ~55% | SKIP |

---

## Effort Summary by Phase

| Phase | Description | New Test Files | New Test Cases | Lines Covered | Coverage |
|-------|-------------|---------------|---------------|---------------|----------|
| Current | Baseline | 15 | ~180 | 5,387 | 35.0% |
| 1 | Quick Wins | +3 | +122 | +1,235 | **43%** |
| 2 | Mock Framework + Unit Tests | +8 | +150 | +3,087 | **63%** |
| 3 | Protocol Integration Tests | +5 | +41 | +715 | **71%** |
| 4 | Hard Modules (Refactor) | +5 | +78 | +1,920 | **83%** |
| 5 | Polish + Mop-up | +4 | +32 | +516 | **~90%** |
| **Total** | | **+25 files** | **+423 tests** | **+7,473** | **~90%** |

---

## Critical Path

```
Phase 1 (no dependencies)
    ├── Extend 6 existing test files
    ├── Create test_tabgroup.c
    └── Create test_focus_nav.c

Phase 2 (depends on: tests/mocks/ creation)
    ├── Create shared mock framework
    ├── 8 new test files using mocks
    └── Extend test_ipc.c with wlroots stubs

Phase 3 (depends on: harness protocol headers)
    ├── Generate client protocol headers
    ├── Extend harness for protocol binding
    └── 5 protocol integration test files

Phase 4 (depends on: keyboard.c refactoring OR Phase 2 mocks)
    ├── Refactor keyboard.c (optional, +15% vs mocks alone)
    ├── 5 test files for hard modules
    └── Rewrite test_placement.c

Phase 5 (depends on: Phase 2 mocks)
    └── 4 remaining test files + extensions
```

Phases 1 and the mock framework creation can start immediately in parallel.
Total estimated effort: **~25 new test files, ~423 new test cases**.
