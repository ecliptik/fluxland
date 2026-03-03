# Test infrastructure for fluxland
# Each test is a standalone executable that returns 0 on success.

test_inc = include_directories('../include', '../src')

# --- test_config: config parser tests ---
test_config = executable(
  'test_config',
  files('test_config.c', '../src/config.c', '../src/rcparser.c'),
  include_directories: test_inc,
  dependencies: [],
  install: false,
)
test('config', test_config)

# --- test_keybind: keybinding parser tests ---
test_keybind = executable(
  'test_keybind',
  files('test_keybind.c', '../src/keybind.c'),
  include_directories: test_inc,
  dependencies: [wayland_server, wlroots, xkbcommon],
  install: false,
)
test('keybind', test_keybind)

# --- test_style: style parser tests ---
test_style = executable(
  'test_style',
  files('test_style.c', '../src/style.c', '../src/rcparser.c'),
  include_directories: test_inc,
  dependencies: [],
  install: false,
)
test('style', test_style)

# --- test_rules: window rules parser tests ---
test_rules = executable(
  'test_rules',
  files('test_rules.c', '../src/rules.c'),
  include_directories: test_inc,
  dependencies: [server_protos, wayland_server, wlroots, xkbcommon, pangocairo, libdrm],
  install: false,
)
test('rules', test_rules)

# --- test_placement: placement logic and edge-snapping tests ---
# placement.c is #included directly in the test to allow stubbing wlroots deps.
test_placement = executable(
  'test_placement',
  files('test_placement.c'),
  include_directories: test_inc,
  dependencies: [wlroots, math],
  install: false,
)
test('placement', test_placement)

# --- test_menu: menu parser tests ---
test_menu = executable(
  'test_menu',
  files('test_menu.c', '../src/menu.c', '../src/menu_parse.c', '../src/menu_render.c', '../src/pixel_buffer.c', '../src/perf.c'),
  include_directories: test_inc,
  dependencies: [server_protos, wayland_server, wlroots, xkbcommon, pangocairo, libdrm],
  install: false,
)
test('menu', test_menu)

# --- test_animation: animation system tests ---
# animation.c is #included directly in the test to allow stubbing wlroots deps.
# Needs wlroots in include path so animation.c's #include <wlr/util/log.h> resolves
# (the header guard WLR_UTIL_LOG_H blocks it from being processed).
test_animation = executable(
  'test_animation',
  files('test_animation.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('animation', test_animation)

# --- test_tabgroup: tab group state machine tests ---
# tabgroup.c is #included directly in the test to allow stubbing wlroots deps.
test_tabgroup = executable(
  'test_tabgroup',
  files('test_tabgroup.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('tabgroup', test_tabgroup)

# --- test_workspace: workspace management logic tests ---
# workspace.c is #included directly in the test to allow stubbing wlroots deps.
test_workspace = executable(
  'test_workspace',
  files('test_workspace.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('workspace', test_workspace)

# --- test_view_logic: view pure logic function tests ---
# view.c is #included directly in the test to allow stubbing wlroots deps.
test_view_logic = executable(
  'test_view_logic',
  files('test_view_logic.c'),
  include_directories: test_inc,
  dependencies: [wlroots, math],
  install: false,
)
test('view_logic', test_view_logic)

# --- test_ipc: IPC command dispatch and subscription tests ---
test_ipc = executable(
  'test_ipc',
  files('test_ipc.c', '../src/ipc_commands.c', '../src/ipc.c', '../src/perf.c'),
  include_directories: test_inc,
  dependencies: [server_protos, wayland_server, wlroots, xkbcommon, pangocairo, libdrm],
  install: false,
)
test('ipc', test_ipc)

# --- test_focus_nav: focus navigation state machine tests ---
test_focus_nav = executable(
  'test_focus_nav',
  files('test_focus_nav.c', '../src/focus_nav.c'),
  include_directories: test_inc,
  dependencies: [server_protos, wayland_server, wlroots, xkbcommon, pangocairo, libdrm],
  install: false,
)
test('focus_nav', test_focus_nav)

# --- test_atspi_bridge: AT-SPI accessibility bridge tests ---
test_atspi_bridge = executable(
  'test_atspi_bridge',
  files('test_atspi_bridge.c', '../src/atspi_bridge.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('atspi_bridge', test_atspi_bridge)

# --- test_validate: config validation tests ---
test_validate = executable(
  'test_validate',
  files('test_validate.c', '../src/validate.c', '../src/rcparser.c'),
  include_directories: test_inc,
  dependencies: [],
  install: false,
)
test('validate', test_validate)

# --- test_session_lock_logic: session lock unit tests ---
# session_lock.c is #included directly in the test to allow stubbing wlroots deps.
test_session_lock_logic = executable(
  'test_session_lock_logic',
  files('test_session_lock_logic.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('session_lock_logic', test_session_lock_logic)

# --- test_autostart: autostart module tests ---
test_autostart = executable(
  'test_autostart',
  files('test_autostart.c', '../src/autostart.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('autostart', test_autostart)

# --- test_mousebind: mouse binding parser tests ---
test_mousebind = executable(
  'test_mousebind',
  files('test_mousebind.c', '../src/mousebind.c'),
  include_directories: test_inc,
  dependencies: [wayland_server, wlroots, xkbcommon],
  install: false,
)
test('mousebind', test_mousebind)

# --- test_toolbar_layout: toolbar layout and parsing tests ---
# toolbar.c is #included directly in the test to allow stubbing wlroots/cairo deps.
test_toolbar_layout = executable(
  'test_toolbar_layout',
  files('test_toolbar_layout.c'),
  include_directories: test_inc,
  dependencies: [wlroots, pangocairo, libdrm],
  install: false,
)
test('toolbar_layout', test_toolbar_layout)

# --- test_slit: slit dockapp container tests ---
# slit.c is #included directly in the test to allow stubbing wlroots/XWayland deps.
test_slit = executable(
  'test_slit',
  files('test_slit.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('slit', test_slit)

# --- test_keyboard: keyboard action dispatch and condition evaluation tests ---
# keyboard.c is #included directly in the test to allow stubbing wlroots/wayland deps.
test_keyboard = executable(
  'test_keyboard',
  files('test_keyboard.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('keyboard', test_keyboard)

# --- test_xwayland_logic: XWayland classification, slit docking, and geometry tests ---
# xwayland.c is #included directly in the test to allow stubbing wlroots/XWayland deps.
test_xwayland_logic = executable(
  'test_xwayland_logic',
  files('test_xwayland_logic.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('xwayland_logic', test_xwayland_logic)

# --- test_cursor_snap: cursor snap zone detection and geometry tests ---
# cursor.c is #included directly in the test to allow stubbing wlroots/cairo deps.
test_cursor_snap = executable(
  'test_cursor_snap',
  files('test_cursor_snap.c'),
  include_directories: test_inc,
  dependencies: [wlroots, pangocairo, libdrm, math],
  install: false,
)
test('cursor_snap', test_cursor_snap)

# --- test_menu_interaction: menu interaction logic tests ---
# menu.c is #included directly in the test to allow stubbing wlroots/cairo/pango deps.
test_menu_interaction = executable(
  'test_menu_interaction',
  files('test_menu_interaction.c'),
  include_directories: test_inc,
  dependencies: [wlroots, xkbcommon, pangocairo, libdrm],
  install: false,
)
test('menu_interaction', test_menu_interaction)

# --- test_decoration_logic: decoration hit-testing and layout tests ---
# decoration.c is #included directly in the test to allow stubbing wlroots/cairo deps.
test_decoration_logic = executable(
  'test_decoration_logic',
  files('test_decoration_logic.c'),
  include_directories: test_inc,
  dependencies: [wlroots, pangocairo, libdrm, math],
  install: false,
)
test('decoration_logic', test_decoration_logic)

# --- test_foreign_toplevel: foreign toplevel management unit tests ---
# foreign_toplevel.c is #included directly in the test to allow stubbing wlroots deps.
test_foreign_toplevel = executable(
  'test_foreign_toplevel',
  files('test_foreign_toplevel.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('foreign_toplevel', test_foreign_toplevel)

# --- test_ipc_server: IPC server transport layer tests ---
# ipc.c is #included directly in the test to allow stubbing wlroots/wayland deps.
test_ipc_server = executable(
  'test_ipc_server',
  files('test_ipc_server.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('ipc_server', test_ipc_server)

# --- test_server: server init, reconfigure, and lifecycle tests ---
# server.c is #included directly in the test to allow stubbing wlroots/wayland deps.
test_server = executable(
  'test_server',
  files('test_server.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('server', test_server)

# --- test_systray: system tray layout, hit-testing, and icon management tests ---
# systray.c is #included directly in the test to allow stubbing wlroots/cairo deps.
# Requires libsystemd (or libelogind) for sd-bus type definitions and vtable macros.
if have_systray
  test_systray = executable(
    'test_systray',
    files('test_systray.c'),
    include_directories: test_inc,
    dependencies: [wlroots, pangocairo, libdrm, sdbus_dep],
    c_args: ['-Wno-unused-parameter'],
    install: false,
  )
  test('systray', test_systray)
endif

# --- test_main: main.c entry point integration tests ---
# Invokes the fluxland binary with various CLI flags and checks output/exit codes.
test_main_env = environment()
test_main_env.set('FLUXLAND_BIN', fluxland_exe.full_path())
test_main = executable(
  'test_main',
  files('test_main.c'),
  install: false,
)
test('main', test_main, env: test_main_env, depends: fluxland_exe, timeout: 30)

# --- test_output: output utility function tests ---
# output.c is #included directly in the test to allow stubbing wlroots deps.
test_output_exe = executable(
  'test_output',
  files('test_output.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('output', test_output_exe)

# --- test_output_mgmt: output management tests ---
# output_management.c is #included directly in the test to allow stubbing wlroots deps.
test_output_mgmt = executable(
  'test_output_mgmt',
  files('test_output_mgmt.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('output_mgmt', test_output_mgmt)

# --- test_protocols: protocol handler unit tests ---
# protocols.c is #included directly in the test to allow stubbing wlroots deps.
test_protocols = executable(
  'test_protocols',
  files('test_protocols.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('protocols', test_protocols)

# --- test_integration: headless compositor startup test ---
wayland_client = dependency('wayland-client', required: false)
if wayland_client.found()
  test_integration = executable(
    'test_integration',
    files('test_integration.c'),
    include_directories: test_inc,
    dependencies: [wayland_client],
    install: false,
  )
  test('integration', test_integration, timeout: 30)

  # --- XDG shell client protocol (client-side header for tests) ---
  wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')
  wayland_scanner = find_program('wayland-scanner')

  xdg_shell_xml = wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml'
  xdg_shell_client_h = custom_target(
    'xdg-shell-client-protocol.h',
    input: xdg_shell_xml,
    output: 'xdg-shell-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  xdg_shell_code = custom_target(
    'xdg-shell-protocol.c',
    input: xdg_shell_xml,
    output: 'xdg-shell-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  # --- Layer shell client protocol (wlroots, local XML) ---
  layer_shell_xml = meson.project_source_root() / 'protocols/wlr-layer-shell-unstable-v1.xml'
  layer_shell_client_h = custom_target(
    'wlr-layer-shell-unstable-v1-client-protocol.h',
    input: layer_shell_xml,
    output: 'wlr-layer-shell-unstable-v1-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  layer_shell_code = custom_target(
    'wlr-layer-shell-unstable-v1-protocol.c',
    input: layer_shell_xml,
    output: 'wlr-layer-shell-unstable-v1-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  # --- Session lock client protocol ---
  session_lock_xml = wl_protocol_dir / 'staging/ext-session-lock/ext-session-lock-v1.xml'
  session_lock_client_h = custom_target(
    'ext-session-lock-v1-client-protocol.h',
    input: session_lock_xml,
    output: 'ext-session-lock-v1-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  session_lock_code = custom_target(
    'ext-session-lock-v1-protocol.c',
    input: session_lock_xml,
    output: 'ext-session-lock-v1-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  # --- Idle inhibit client protocol ---
  idle_inhibit_xml = wl_protocol_dir / 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'
  idle_inhibit_client_h = custom_target(
    'idle-inhibit-unstable-v1-client-protocol.h',
    input: idle_inhibit_xml,
    output: 'idle-inhibit-unstable-v1-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  idle_inhibit_code = custom_target(
    'idle-inhibit-unstable-v1-protocol.c',
    input: idle_inhibit_xml,
    output: 'idle-inhibit-unstable-v1-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  # --- Idle notify client protocol ---
  idle_notify_xml = wl_protocol_dir / 'staging/ext-idle-notify/ext-idle-notify-v1.xml'
  idle_notify_client_h = custom_target(
    'ext-idle-notify-v1-client-protocol.h',
    input: idle_notify_xml,
    output: 'ext-idle-notify-v1-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  idle_notify_code = custom_target(
    'ext-idle-notify-v1-protocol.c',
    input: idle_notify_xml,
    output: 'ext-idle-notify-v1-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  # --- Foreign toplevel list client protocol ---
  foreign_toplevel_list_xml = wl_protocol_dir / 'staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml'
  foreign_toplevel_list_client_h = custom_target(
    'ext-foreign-toplevel-list-v1-client-protocol.h',
    input: foreign_toplevel_list_xml,
    output: 'ext-foreign-toplevel-list-v1-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  foreign_toplevel_list_code = custom_target(
    'ext-foreign-toplevel-list-v1-protocol.c',
    input: foreign_toplevel_list_xml,
    output: 'ext-foreign-toplevel-list-v1-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  # --- Integration test harness library ---
  integration_inc = include_directories('integration')
  harness_lib = static_library(
    'test_harness',
    files('integration/harness.c'),
    xdg_shell_client_h,
    xdg_shell_code,
    include_directories: [test_inc, integration_inc],
    dependencies: [wayland_client],
    install: false,
  )

  harness_dep = declare_dependency(
    link_with: harness_lib,
    sources: [xdg_shell_client_h],
    include_directories: [integration_inc],
  )

  # --- test_xdg_shell: XDG shell protocol tests ---
  test_xdg_shell = executable(
    'test_xdg_shell',
    files('test_xdg_shell.c'),
    xdg_shell_client_h,
    include_directories: [test_inc, integration_inc],
    dependencies: [wayland_client, harness_dep],
    install: false,
  )
  test('xdg_shell', test_xdg_shell, timeout: 30)

  # --- test_multi_client: multi-client integration tests ---
  test_multi_client = executable(
    'test_multi_client',
    files('test_multi_client.c'),
    xdg_shell_client_h,
    include_directories: [test_inc, integration_inc],
    dependencies: [wayland_client, harness_dep],
    install: false,
  )
  test('multi_client', test_multi_client, timeout: 30)

  # --- test_layer_shell: layer shell protocol tests ---
  test_layer_shell = executable(
    'test_layer_shell',
    files('test_layer_shell.c'),
    layer_shell_client_h,
    layer_shell_code,
    include_directories: [test_inc, integration_inc],
    dependencies: [wayland_client, harness_dep],
    c_args: ['-Wno-unused-parameter'],
    install: false,
  )
  test('layer_shell', test_layer_shell, timeout: 10)

  # --- test_session_lock: session lock protocol tests ---
  test_session_lock = executable('test_session_lock',
    files('test_session_lock.c'),
    session_lock_client_h, session_lock_code,
    include_directories: [test_inc, integration_inc],
    dependencies: [wayland_client, harness_dep],
    c_args: ['-Wno-unused-parameter'],
    install: false,
  )
  test('session_lock', test_session_lock, timeout: 10)

  # --- test_idle: idle inhibit protocol tests ---
  test_idle = executable('test_idle',
    files('test_idle.c'),
    idle_inhibit_client_h, idle_inhibit_code,
    include_directories: [test_inc, integration_inc],
    dependencies: [wayland_client, harness_dep],
    c_args: ['-Wno-unused-parameter'],
    install: false,
  )
  test('idle', test_idle, timeout: 10)
endif

# --- Visual regression tests (decoration rendering) ---
visual_inc = include_directories('visual')
image_compare_lib = static_library(
  'image_compare',
  files('visual/image_compare.c'),
  include_directories: [test_inc, visual_inc],
  dependencies: [pangocairo, math],
  install: false,
)

image_compare_dep = declare_dependency(
  link_with: image_compare_lib,
  include_directories: [visual_inc],
)

test_deco_deps = [wlroots, pangocairo, math, image_compare_dep]
if have_pixbuf
  test_deco_deps += [pixbuf_dep]
endif
test_decorations = executable(
  'test_decorations',
  files('visual/test_decorations.c', '../src/render.c', '../src/style.c',
        '../src/rcparser.c', '../src/perf.c'),
  include_directories: [test_inc, visual_inc],
  dependencies: test_deco_deps,
  install: false,
)
test('decorations', test_decorations, timeout: 30)

# --- test_input: input device management tests ---
# input.c is #included directly in the test to allow stubbing wlroots deps.
test_input = executable(
  'test_input',
  files('test_input.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('input', test_input)

# --- test_text_input: text input v3 protocol tests ---
# text_input.c is #included directly in the test to allow stubbing wlroots deps.
test_text_input = executable(
  'test_text_input',
  files('test_text_input.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('text_input', test_text_input)

# --- test_tablet: tablet device support tests ---
# tablet.c is #included directly in the test to allow stubbing wlroots deps.
test_tablet = executable(
  'test_tablet',
  files('test_tablet.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('tablet', test_tablet)

# --- test_render: text rendering and Pango/Cairo tests ---
test_render_deps = [wlroots, pangocairo, libdrm, math]
if have_pixbuf
  test_render_deps += [pixbuf_dep]
endif
test_render = executable(
  'test_render',
  files('test_render.c'),
  include_directories: test_inc,
  dependencies: test_render_deps,
  install: false,
)
test('render', test_render)

# --- test_rcparser: RC config file parser tests ---
test_rcparser = executable(
  'test_rcparser',
  files('test_rcparser.c'),
  include_directories: test_inc,
  dependencies: [],
  install: false,
)
test('rcparser', test_rcparser)

# --- test_util: utility function tests ---
# util.c includes <wlr/util/log.h> — needs wlroots in include path even though guarded.
test_util = executable(
  'test_util',
  files('test_util.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('util', test_util)

# --- test_screencopy: screen capture protocol tests ---
# screencopy.c is #included directly in the test to allow stubbing wlroots deps.
test_screencopy = executable(
  'test_screencopy',
  files('test_screencopy.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('screencopy', test_screencopy)

# --- test_drm_lease: DRM lease protocol tests ---
# drm_lease.c is #included directly in the test to allow stubbing wlroots deps.
test_drm_lease = executable(
  'test_drm_lease',
  files('test_drm_lease.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('drm_lease', test_drm_lease)

# --- test_gamma_control: gamma control protocol tests ---
# gamma_control.c is #included directly in the test to allow stubbing wlroots deps.
test_gamma_control = executable(
  'test_gamma_control',
  files('test_gamma_control.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('gamma_control', test_gamma_control)

# --- test_transient_seat: transient seat protocol tests ---
# transient_seat.c is #included directly in the test to allow stubbing wlroots deps.
test_transient_seat = executable(
  'test_transient_seat',
  files('test_transient_seat.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('transient_seat', test_transient_seat)

# --- test_small_protocols: fractional_scale, presentation, viewporter, drm_syncobj tests ---
test_small_protocols = executable(
  'test_small_protocols',
  files('test_small_protocols.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('small_protocols', test_small_protocols)

# --- test_perf: performance profiling infrastructure tests ---
# perf.c is #included directly in the test with WM_PERF_ENABLE defined.
test_perf = executable(
  'test_perf',
  files('test_perf.c'),
  include_directories: test_inc,
  dependencies: [wlroots],
  install: false,
)
test('perf', test_perf)

# --- test_gesture: touchpad gesture state machine tests ---
test_gesture = executable(
  'test_gesture',
  files('test_gesture.c'),
  include_directories: test_inc,
  dependencies: [server_protos, wayland_server, wlroots, xkbcommon, pangocairo, libdrm],
  install: false,
)
test('gesture', test_gesture)

# --- test_screen_cast: screen cast module unit tests ---
# screen_cast.c is #included directly in the test to test stub path.
test_screen_cast = executable(
  'test_screen_cast',
  files('test_screen_cast.c'),
  include_directories: test_inc,
  dependencies: [],
  install: false,
)
test('screen_cast_unit', test_screen_cast)

# --- Fuzz targets (requires -fsanitize=fuzzer support) ---
subdir('fuzz')
