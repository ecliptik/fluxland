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

# --- test_style: style/theme parser tests ---
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

# --- test_placement: placement grid math tests ---
test_placement = executable(
  'test_placement',
  files('test_placement.c'),
  include_directories: test_inc,
  dependencies: [math],
  install: false,
)
test('placement', test_placement)

# --- test_menu: menu parser tests ---
test_menu = executable(
  'test_menu',
  files('test_menu.c', '../src/menu.c'),
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
  files('test_ipc.c', '../src/ipc_commands.c', '../src/ipc.c'),
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

# --- test_validate: config validation tests ---
test_validate = executable(
  'test_validate',
  files('test_validate.c', '../src/validate.c', '../src/rcparser.c'),
  include_directories: test_inc,
  dependencies: [],
  install: false,
)
test('validate', test_validate)

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

test_decorations = executable(
  'test_decorations',
  files('visual/test_decorations.c', '../src/render.c', '../src/style.c',
        '../src/rcparser.c'),
  include_directories: [test_inc, visual_inc],
  dependencies: [pangocairo, math, image_compare_dep],
  install: false,
)
test('decorations', test_decorations, timeout: 30)

# --- Fuzz targets (requires -fsanitize=fuzzer support) ---
subdir('fuzz')
