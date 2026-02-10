# Test infrastructure for wm-wayland
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
endif
