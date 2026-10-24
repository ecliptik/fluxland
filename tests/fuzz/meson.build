# Fuzz targets for fluxland parsers
#
# These require a compiler with -fsanitize=fuzzer support (typically clang).
# They are built as standalone executables that can be run with:
#   ./fuzz_rcparser tests/fuzz/corpus/rcparser/
#   ./fuzz_keybind tests/fuzz/corpus/keybind/

fuzz_inc = include_directories('../../src')

# Check if the compiler supports -fsanitize=fuzzer
have_fuzzer = cc.has_argument('-fsanitize=fuzzer')

if have_fuzzer
  fuzz_args = ['-fsanitize=fuzzer']
  fuzz_link_args = ['-fsanitize=fuzzer']

  # --- fuzz_rcparser: rcparser config file fuzzer ---
  executable(
    'fuzz_rcparser',
    files('fuzz_rcparser.c', '../../src/rcparser.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [],
    install: false,
  )

  # --- fuzz_keybind: keybinding parser fuzzer ---
  executable(
    'fuzz_keybind',
    files('fuzz_keybind.c', '../../src/keybind.c', '../../src/command.c',
      '../../src/client_pattern.c',
      '../../src/regex_guard.c', 'fuzz_stubs.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [wayland_server, wlroots, xkbcommon, server_protos,
      pangocairo],
    install: false,
  )

  # --- fuzz_style: style file fuzzer ---
  executable(
    'fuzz_style',
    files('fuzz_style.c', '../../src/style.c', '../../src/rcparser.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [],
    install: false,
  )

  # --- fuzz_menu: menu file parser fuzzer ---
  executable(
    'fuzz_menu',
    files('fuzz_menu.c', 'menu_stubs.c', 'view_stubs.c', '../../src/menu.c',
      '../../src/menu_parse.c', '../../src/menu_render.c',
      '../../src/pixel_buffer.c', '../../src/perf.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [server_protos, wayland_server, wlroots, xkbcommon, pangocairo, libdrm],
    install: false,
  )

  # --- fuzz_ipc_command: IPC JSON command parser fuzzer ---
  executable(
    'fuzz_ipc_command',
    files('fuzz_ipc_command.c', '../../src/ipc_json.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [],
    install: false,
  )

  # --- fuzz_rules: window rules/apps file parser fuzzer ---
  executable(
    'fuzz_rules',
    files('fuzz_rules.c', '../../src/rules.c',
      '../../src/regex_guard.c', 'view_stubs.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [wayland_server, wlroots, server_protos, pangocairo],
    install: false,
  )

  # --- fuzz_mousebind: mouse binding config parser fuzzer ---
  executable(
    'fuzz_mousebind',
    files('fuzz_mousebind.c', '../../src/mousebind.c', '../../src/keybind.c', '../../src/command.c',
      '../../src/client_pattern.c',
      '../../src/regex_guard.c', 'fuzz_stubs.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [wayland_server, wlroots, xkbcommon, server_protos,
      pangocairo],
    install: false,
  )

  # --- fuzz_command: command language + client pattern fuzzer ---
  executable(
    'fuzz_command',
    files('fuzz_command.c', '../../src/command.c',
      '../../src/client_pattern.c',
      '../../src/regex_guard.c', '../../src/keybind.c', 'fuzz_stubs.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [wayland_server, wlroots, xkbcommon, server_protos,
      pangocairo],
    install: false,
  )
else
  message('Fuzz targets disabled: compiler does not support -fsanitize=fuzzer')
endif

# Install seed corpus files regardless of fuzzer support
# (useful for manual testing or CI with external fuzzers)
