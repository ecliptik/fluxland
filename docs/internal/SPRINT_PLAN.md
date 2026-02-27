# Fluxland Sprint Plan — Window Management, Testing & Quality of Life

**Created:** 2026-02-24
**Scope:** 11 TODO items across 3 categories
**Sprints:** 6 sprints, each completable in a single Claude Code session

---

## Dependency Graph

```
Sprint 1: QoL Quick Wins ─────────────────────────────────┐
Sprint 2: Window Animations ──────────────────────────────┤
Sprint 3: Window Snap Zones ──────────────────────────────┤── All independent
Sprint 4: Test Foundation ────────────────────────────────┤
Sprint 5: Per-Output Workspaces ──────────────────────────┘
                                                          │
Sprint 6: Test Expansion ─────────── depends on Sprint 4 ─┘
```

**Parallelization:** Sprints 1-5 have zero cross-dependencies and can all
run as separate Claude Code sessions simultaneously. Sprint 6 requires
Sprint 4's test harness to be merged first.

---

## Sprint 1: Quality of Life Bundle

**Goal:** Implement all 4 QoL improvements in one session
**Estimated LOC:** ~800 new/modified lines
**Complexity:** S-M (all items are small, well-scoped)
**Files touched:** 12-15 files

### Deliverables

1. **Mouse Button Remapping** (~100 LOC)
   - Add `mouse_button_map[6]` to `wm_config` struct
   - Parse `session.mouse.button[1-5].map` in config.c
   - Modify `button_number_to_code()` in mousebind.c to use mapping
   - Add example config to `data/init`

2. **IPC Event Subscriptions** (~100 LOC)
   - Add `WM_IPC_EVENT_STYLE_CHANGED`, `WM_IPC_EVENT_CONFIG_RELOADED` to enum in ipc.h
   - Broadcast style event after `style_load()` in `wm_server_reconfigure()`
   - Broadcast config event after `config_load()` in `wm_server_reconfigure()`
   - Extend IPC subscribe handler to accept new event names

3. **Config Validation Tool** (~400 LOC)
   - Define `rc_validation_error` and `rc_validation_result` structs
   - Implement validators: range checks, enum checks, file existence
   - Per-subsystem: config.c, keybind.c, mousebind.c, style.c validators
   - Add `--check-config` flag to main.c
   - Exit 0 on valid, exit 1 on errors, warnings printed with exit 0

4. **Runtime Reload Improvements** (~200 LOC)
   - Audit `wm_server_reconfigure()` for missing reload calls
   - Fix mousebind reload if missing (potential existing bug)
   - Add toolbar repositioning on placement/height change
   - Add slit repositioning on placement/layer change
   - Rebuild workspace menu on name changes

### Key Files
| File | Changes |
|------|---------|
| `src/config.c/h` | Mouse mapping, validation, reload audit |
| `src/ipc.h` | New event enum values |
| `src/ipc_commands.c` | Subscribe handler for new events |
| `src/server.c` | Broadcast calls, reload improvements |
| `src/mousebind.c` | Button mapping table lookup |
| `src/main.c` | `--check-config` CLI flag |
| `src/rcparser.c/h` | Generic validation helpers |
| `src/keybind.c` | Keybind validation function |
| `src/style.c` | Style validation function |
| `src/toolbar.c/h` | Refresh/reposition function |
| `src/slit.c/h` | Refresh/reposition function |

### Acceptance Criteria
- [ ] `fluxland --check-config` validates all config files and reports errors
- [ ] `fluxland --check-config` exits 0 on valid config, 1 on invalid
- [ ] Mouse button remapping works via `session.mouse.button1.map: BTN_RIGHT` (swaps left/right)
- [ ] `fluxland-ctl subscribe style_changed` receives events on reconfigure
- [ ] `fluxland-ctl subscribe config_reloaded` receives events on reconfigure
- [ ] Changing toolbar placement in config + reconfigure repositions toolbar without restart
- [ ] Existing unit tests pass
- [ ] New tests for validation and mouse mapping

### Getting Started
```bash
# 1. Read these files first to understand current state:
cat src/config.c src/config.h     # Config struct and parsing
cat src/ipc.h                      # IPC event enum
cat src/server.c                   # wm_server_reconfigure()
cat src/mousebind.c                # button_number_to_code()
cat src/main.c                     # CLI argument parsing

# 2. Build and run existing tests to establish baseline:
ninja -C build && meson test -C build --print-errorlogs

# 3. Recommended implementation order:
#    Mouse mapping → IPC events → Runtime reload → Config validation
#    (easiest to hardest, each builds on understanding from previous)
```

---

## Sprint 2: Window Animations

**Goal:** Add fade in/out animations for window map/unmap and minimize/restore
**Estimated LOC:** ~350-450 new lines
**Complexity:** M
**Files touched:** 5-7 files

### Deliverables

1. **Animation Infrastructure** (~100 LOC)
   - Create `animation.h` with animation struct and types
   - Timer-based animation using `wl_event_loop_add_timer()`
   - Linear interpolation (with optional ease-in/out)
   - Animation cancellation on user interaction or destroy

2. **Fade In on Map** (~60 LOC)
   - Start at opacity 0, animate to target alpha over configurable duration
   - Hook into `handle_xdg_toplevel_map()` in view.c
   - Cancel animation if view is destroyed during fade

3. **Fade Out on Unmap** (~80 LOC)
   - Animate from current opacity to 0
   - Defer actual scene tree cleanup until animation completes
   - Handle edge case: window destroyed during fade-out

4. **Minimize/Restore Effects** (~50 LOC)
   - Fade out on minimize, fade in on restore
   - Reuse fade animation functions
   - Call `wlr_scene_node_set_enabled(false)` after fade-out completes

5. **Configuration** (~50 LOC)
   - `animateWindowMap: true/false`
   - `animateWindowUnmap: true/false`
   - `animateMinimize: true/false`
   - `animationDuration: 300` (milliseconds)
   - Parse in rcparser/config.c

### Key Files
| File | Changes |
|------|---------|
| `src/animation.h` | **NEW** — animation structs, API |
| `src/animation.c` | **NEW** — timer callbacks, interpolation |
| `src/view.c` | Hook fade in map handler, fade out unmap |
| `src/view.h` | Animation pointer in view struct |
| `src/config.c/h` | Animation config options |
| `src/rcparser.c` | Parse animation settings |
| `src/meson.build` | Add animation.c to sources |

### Risks & Mitigations
- **Race condition:** View destroyed during animation → cancel animation in destroy handler, NULL-check view in timer callback
- **Performance:** Many simultaneous animations → limit concurrent count or use centralized tick
- **Frame alignment:** Timer not synced to vblank → acceptable for fade effects, would matter more for position animations

### Acceptance Criteria
- [ ] Windows fade in when mapped (opacity 0 → target over 300ms)
- [ ] Windows fade out when unmapped (opacity → 0 over 300ms, then cleaned up)
- [ ] Minimize fades out, restore fades in
- [ ] Setting `animateWindowMap: false` disables map animation
- [ ] No crashes when rapidly opening/closing windows
- [ ] No memory leaks (animation structs freed on completion or cancellation)
- [ ] Existing tests pass

### Getting Started
```bash
# 1. Read these files to understand current state:
cat src/view.c     # handle_xdg_toplevel_map(), wm_view_set_opacity()
cat src/view.h     # view struct (focus_alpha, unfocus_alpha)
cat src/cursor.c   # auto_raise_timer pattern (timer infrastructure example)
cat src/server.h   # server struct, event loop access
cat src/config.h   # existing config options

# 2. Study the existing timer pattern:
grep -n "wl_event_loop_add_timer\|wl_event_source_timer_update" src/*.c

# 3. Study the existing opacity API:
grep -n "wm_view_set_opacity\|set_opacity" src/view.c

# 4. Build baseline:
ninja -C build && meson test -C build --print-errorlogs
```

---

## Sprint 3: Window Snap Zones

**Goal:** Add Windows/macOS-style window snapping to screen edges and corners
**Estimated LOC:** ~500-700 new lines
**Complexity:** M-L
**Files touched:** 5-7 files

### Deliverables

1. **Snap Zone Detection** (~120 LOC)
   - Define `enum wm_snap_zone` (NONE, LEFT, RIGHT, TOP, BOTTOM, corners, halves)
   - During `process_cursor_motion()` in MOVE mode, check cursor proximity to output edges
   - Use configurable threshold (default 10px)
   - Track `server->snapped_zone` and `server->snap_preview_box`

2. **Visual Preview Overlay** (~200 LOC)
   - Create semi-transparent Cairo scene buffer showing snap target geometry
   - Render 2-4px border outline with 30% opacity fill
   - Use highlight color from style
   - Attach to `layer_overlay` scene tree (above windows, below menus)
   - Update position/size on cursor motion, destroy on move end

3. **Snap Application** (~120 LOC)
   - On mouse button release in MOVE mode with active snap zone:
     - Calculate target geometry from snap zone + output usable area
     - Apply geometry via `wlr_xdg_toplevel_set_size()` + `wlr_scene_node_set_position()`
   - Half zones: 50% width, full height
   - Quarter/corner zones: 50% width, 50% height
   - Destroy preview overlay

4. **Configuration** (~60 LOC)
   - `enableWindowSnapping: true/false`
   - `snapThreshold: 10` (pixels from edge to trigger)
   - `snapToWindows: false` (future: snap to adjacent windows)
   - Parse in config.c

### Key Files
| File | Changes |
|------|---------|
| `src/cursor.c` | Snap detection in process_cursor_motion(), preview management |
| `src/view.c` | Snap application on move end |
| `src/render.c` | Preview overlay rendering function |
| `src/server.h` | Snap state fields (zone, preview buffer, config) |
| `src/config.c/h` | Snap configuration options |
| `src/output.c` | Output usable area for snap geometry calculation |

### Risks & Mitigations
- **Performance:** Edge-distance checks on every motion event → only check current output, simple math (negligible)
- **Preview flicker:** Creating/destroying Cairo surfaces frequently → reuse buffer, only update position
- **Multi-output:** Snap zones only on current output; crossing output boundary clears snap state
- **Edge cases:** Decorated windows larger than snap zone → clamp to output bounds

### Acceptance Criteria
- [ ] Dragging window to left edge of screen shows snap preview (left half)
- [ ] Dragging to right edge shows right half preview
- [ ] Dragging to corners shows quarter preview
- [ ] Releasing mouse while preview shown snaps window to target geometry
- [ ] Preview disappears when cursor moves away from edge
- [ ] `enableWindowSnapping: false` disables all snapping
- [ ] Works correctly on multi-output setups (snaps to correct output)
- [ ] Existing tests pass

### Getting Started
```bash
# 1. Read these files to understand move/resize:
cat src/cursor.c    # process_cursor_motion(), handle_cursor_button()
cat src/view.c      # wm_view_begin_interactive()
cat src/server.h    # grab_x, grab_y, grab_geobox, cursor_mode enum

# 2. Understand scene graph structure:
grep -n "layer_overlay\|view_layer_normal\|scene_tree" src/server.h

# 3. Study existing overlay rendering:
grep -n "position_overlay\|wlr_scene_buffer_create" src/*.c

# 4. Study decoration rendering for Cairo buffer pattern:
cat src/decoration.c | head -100  # Buffer creation pattern

# 5. Build baseline:
ninja -C build && meson test -C build --print-errorlogs
```

---

## Sprint 4: Test Foundation

**Goal:** Build integration test harness + basic protocol tests + fuzzing infrastructure
**Estimated LOC:** ~600-800 new lines
**Complexity:** M
**Files touched:** 8-12 files (mostly new)

### Deliverables

1. **Integration Test Harness** (~200 LOC)
   - `tests/integration/harness.c/h` — Wayland client helpers
   - Connect to compositor via socket
   - Registry binding for wl_compositor, xdg_wm_base, wl_seat
   - Window creation/mapping/unmapping helpers
   - Roundtrip synchronization
   - Setup/teardown with compositor fork + headless backend

2. **Basic Protocol Tests** (~200 LOC)
   - `test_xdg_shell_basic` — window create → map → commit → unmap
   - `test_xdg_shell_properties` — set title, app_id
   - `test_xdg_shell_states` — maximize, fullscreen state transitions
   - `test_focus_basic` — two windows, verify focus changes
   - `test_workspace_switch` — switch workspace, verify window visibility
   - Expand existing `test_integration.c` or create new test files

3. **Fuzzing Infrastructure** (~200 LOC)
   - `tests/fuzz/fuzz_rcparser.c` — libFuzzer target for rcparser.c
   - `tests/fuzz/fuzz_keybind.c` — libFuzzer target for keybind.c
   - `tests/fuzz/meson.build` — build fuzz targets with sanitizers
   - Seed corpus: valid config/keys files from `data/`
   - `meson test -C build --suite fuzzing` runs short fuzz sessions

4. **Build System Updates** (~50 LOC)
   - Update `tests/meson.build` for new integration tests
   - Add `tests/fuzz/meson.build` for fuzz targets
   - Ensure headless backend + pixman renderer available for CI

### Key Files
| File | Changes |
|------|---------|
| `tests/integration/harness.c` | **NEW** — Wayland client test helpers |
| `tests/integration/harness.h` | **NEW** — Harness API |
| `tests/integration/test_xdg_shell.c` | **NEW** — XDG shell protocol tests |
| `tests/integration/test_focus.c` | **NEW** — Focus policy tests |
| `tests/integration/test_workspace.c` | **NEW** — Workspace switching tests |
| `tests/fuzz/fuzz_rcparser.c` | **NEW** — rcparser fuzz target |
| `tests/fuzz/fuzz_keybind.c` | **NEW** — keybind fuzz target |
| `tests/fuzz/meson.build` | **NEW** — Fuzz target build rules |
| `tests/meson.build` | Add integration test targets |

### Risks & Mitigations
- **Headless backend:** Must work reliably for CI → test locally first, document env requirements
- **Protocol timing:** Async Wayland events → use `wl_display_roundtrip()` liberally
- **Fuzzer dependencies:** libFuzzer requires Clang → make fuzz targets optional (`if get_option('fuzzing')`)
- **Test isolation:** Each test forks compositor → ensure cleanup on timeout/crash

### Acceptance Criteria
- [ ] `meson test -C build` runs all existing + new integration tests
- [ ] Integration tests create real windows via xdg_shell protocol
- [ ] At least 5 new protocol-level integration tests pass
- [ ] `fuzz_rcparser` runs for 30s without crashes on seed corpus
- [ ] `fuzz_keybind` runs for 30s without crashes on seed corpus
- [ ] All existing unit tests still pass
- [ ] Tests work in headless mode (`WLR_BACKENDS=headless WLR_RENDERER=pixman`)

### Getting Started
```bash
# 1. Read existing test infrastructure:
cat tests/meson.build
cat tests/test_integration.c    # Current integration test (minimal)
cat tests/test_config.c          # Example unit test pattern

# 2. Understand compositor startup for headless:
grep -n "WLR_BACKENDS\|headless\|pixman" src/*.c
cat src/server.c                 # wm_server_init()

# 3. Read parsers to understand fuzz targets:
cat src/rcparser.c               # Simple parser, 192 lines
cat src/keybind.c | head -100    # Complex parser, understand entry point

# 4. Check existing protocol bindings:
cat protocols/meson.build

# 5. Build baseline:
ninja -C build && meson test -C build --print-errorlogs
```

---

## Sprint 5: Per-Output Workspaces

**Goal:** Implement per-output workspace display (Option A: global workspaces, per-output active workspace)
**Estimated LOC:** ~300-400 new lines
**Complexity:** M
**Files touched:** 6-8 files

### Deliverables

1. **Per-Output Workspace State** (~80 LOC)
   - Add `displayed_workspace` pointer to `wm_output` struct
   - Initialize to global default workspace on output creation
   - Track focused output (output containing focused view, or output under cursor)

2. **Modified Workspace Switching** (~120 LOC)
   - `wm_workspace_switch()` now takes optional output parameter
   - When switching: only change `displayed_workspace` on target output
   - Scene tree management: per-output enable/disable of workspace trees
   - Keybindings (NextWorkspace/PrevWorkspace) switch on focused output

3. **View Placement** (~60 LOC)
   - New windows placed on focused output's current workspace
   - `wm_view_move_to_workspace()` updated for per-output context
   - SendToWorkspace command works relative to current output

4. **Configuration** (~40 LOC)
   - `workspaceMode: global|per-output` (default: global for backward compat)
   - When `global`: existing behavior preserved exactly
   - When `per-output`: per-output workspace switching enabled

5. **IPC Updates** (~50 LOC)
   - Workspace events include output identifier
   - Foreign toplevel protocol updated with per-output workspace state

### Key Files
| File | Changes |
|------|---------|
| `src/workspace.c/h` | Switch function takes output, per-output logic |
| `src/output.c/h` | Add displayed_workspace field, focused output tracking |
| `src/server.c/h` | Focused output resolution, config |
| `src/view.c` | View placement on correct output's workspace |
| `src/keyboard.c` | Workspace commands use focused output |
| `src/config.c/h` | workspaceMode option |
| `src/ipc_commands.c` | Output-aware workspace events |

### Risks & Mitigations
- **Sticky windows:** Must remain visible across all outputs → keep sticky_tree always enabled, independent of workspace switching
- **Focus semantics:** Which output is "focused"? → Use output containing focused view; fallback to output under cursor
- **Scene graph visibility:** Disabling workspace tree on one output must not affect another → may need per-output scene tree cloning or viewport-based clipping
- **Backward compatibility:** `workspaceMode: global` must behave identically to current code

### Acceptance Criteria
- [ ] `workspaceMode: global` behaves identically to current (regression test)
- [ ] `workspaceMode: per-output` allows independent workspace switching per monitor
- [ ] Sticky windows visible on all outputs regardless of workspace
- [ ] NextWorkspace/PrevWorkspace operate on the focused output
- [ ] New windows open on the focused output's active workspace
- [ ] SendToWorkspace moves window within current output
- [ ] IPC workspace events include output info
- [ ] Existing tests pass

### Getting Started
```bash
# 1. Read workspace implementation:
cat src/workspace.c src/workspace.h    # Core workspace logic
cat src/output.c src/output.h          # Output management

# 2. Understand scene graph structure:
grep -n "view_layer_normal\|workspace.*tree\|sticky_tree" src/server.h
grep -n "wlr_scene_node_set_enabled\|wm_workspace_switch" src/workspace.c

# 3. Understand workspace commands:
grep -n "WORKSPACE\|NextWorkspace\|PrevWorkspace\|SendTo" src/keyboard.c

# 4. Understand view placement:
grep -n "wm_view_move_to_workspace\|current_workspace" src/view.c

# 5. Build baseline:
ninja -C build && meson test -C build --print-errorlogs
```

---

## Sprint 6: Test Expansion

**Goal:** Expand test coverage with multi-client tests, more fuzz targets, and screenshot comparison
**Estimated LOC:** ~600-800 new lines
**Complexity:** M-L
**Depends on:** Sprint 4 (test harness must be merged)
**Files touched:** 10-15 files (mostly new)

### Deliverables

1. **Multi-Client Integration Tests** (~200 LOC)
   - `test_multiple_windows` — multiple clients, focus ordering
   - `test_pointer_focus` — decoration surface picking
   - `test_tab_groups` — window grouping, tab navigation
   - `test_stacking_order` — z-order enforcement, raise/lower
   - `test_window_rules` — rules applied at map time
   - `test_move_resize` — geometry changes via protocol

2. **Additional Fuzz Targets** (~150 LOC)
   - `fuzz_style.c` — style file parser fuzzing
   - `fuzz_rules.c` — window rules parser fuzzing
   - `fuzz_menu.c` — menu file parser fuzzing
   - Expanded seed corpus for all targets

3. **Screenshot Comparison Framework** (~250 LOC)
   - `tests/visual/image_compare.c/h` — PNG load + pixel comparison
   - `tests/visual/test_decorations.c` — basic decoration rendering tests
   - Screencopy client for capturing compositor output
   - Reference image generation script
   - Fuzzy comparison with configurable tolerance (default 1%)

4. **WLCS Preparation** (~100 LOC, if time permits)
   - `src/wlcs_integration.c` stub — WlcsServerIntegration struct
   - Non-blocking event loop wrapper
   - Build as shared library target

### Key Files
| File | Changes |
|------|---------|
| `tests/integration/test_multi_client.c` | **NEW** |
| `tests/integration/test_tab_groups.c` | **NEW** |
| `tests/integration/test_window_rules.c` | **NEW** |
| `tests/fuzz/fuzz_style.c` | **NEW** |
| `tests/fuzz/fuzz_rules.c` | **NEW** |
| `tests/fuzz/fuzz_menu.c` | **NEW** |
| `tests/visual/image_compare.c/h` | **NEW** |
| `tests/visual/test_decorations.c` | **NEW** |
| `tests/visual/reference/` | **NEW** — reference PNGs |
| `tests/meson.build` | Add new test targets |

### Acceptance Criteria
- [ ] 6+ new multi-client integration tests pass
- [ ] Style, rules, and menu fuzz targets run without crashes
- [ ] Screenshot test captures decoration rendering and compares to reference
- [ ] Image comparison tolerance handles minor font rendering differences
- [ ] All fuzz targets can run via `meson test --suite fuzzing`
- [ ] All existing tests still pass

### Getting Started
```bash
# 1. Verify Sprint 4 harness is available:
cat tests/integration/harness.h    # Should exist from Sprint 4
cat tests/fuzz/meson.build         # Should exist from Sprint 4

# 2. Read multi-client related code:
cat src/view.c | grep -A20 "wm_focus_view"   # Focus logic
cat src/tabgroup.c | head -50                  # Tab group API
cat src/rules.c | head -50                     # Rules API

# 3. Read screencopy for screenshot testing:
cat src/screencopy.c

# 4. Read decoration rendering for visual tests:
cat src/decoration.c | head -100

# 5. Build baseline:
ninja -C build && meson test -C build --print-errorlogs
```

---

## Sprint Summary

| Sprint | Category | Complexity | Est. LOC | Dependencies | Parallelizable |
|--------|----------|-----------|----------|-------------|----------------|
| 1 | Quality of Life | S-M | ~800 | None | Yes |
| 2 | Window Animations | M | ~450 | None | Yes |
| 3 | Window Snap Zones | M-L | ~700 | None | Yes |
| 4 | Test Foundation | M | ~700 | None | Yes |
| 5 | Per-Output Workspaces | M | ~400 | None | Yes |
| 6 | Test Expansion | M-L | ~800 | Sprint 4 | After Sprint 4 |

**Total estimated new code:** ~3,850 lines across all sprints

### Recommended Execution Order

**Wave 1 (parallel):** Sprints 1, 2, 3, 4, 5 — all independent, run simultaneously
**Wave 2 (sequential):** Sprint 6 — after Sprint 4 merges

For maximum throughput: launch 5 Claude sessions simultaneously for Wave 1,
then 1 session for Wave 2.

For incremental delivery: prioritize by user value:
1. Sprint 1 (QoL) — immediate usability improvements
2. Sprint 3 (Snap Zones) — highest user-facing impact
3. Sprint 2 (Animations) — visual polish
4. Sprint 4 (Test Foundation) — engineering quality
5. Sprint 5 (Per-Output Workspaces) — power user feature
6. Sprint 6 (Test Expansion) — long-term stability

### Session Tips for Each Sprint

- Start by reading the "Getting Started" section and building the project
- Run existing tests to establish baseline before making changes
- Implement in the order listed within each sprint (items build on each other)
- Run `meson test -C build --print-errorlogs` after each deliverable
- Commit after each deliverable for easy rollback
- If blocked on one item, skip to the next and return later
