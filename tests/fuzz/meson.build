# Fuzz targets for fluxland parsers
#
# These require a compiler with -fsanitize=fuzzer support (typically clang).
# They are built as standalone executables that can be run with:
#   ./fuzz_rcparser tests/fuzz/corpus/rcparser/
#   ./fuzz_keybind tests/fuzz/corpus/keybind/

fuzz_inc = include_directories('../../include', '../../src')

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
    files('fuzz_keybind.c', '../../src/keybind.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [wayland_server, wlroots, xkbcommon],
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
    files('fuzz_menu.c', '../../src/menu.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [server_protos, wayland_server, wlroots, xkbcommon, pangocairo, libdrm],
    install: false,
  )

  # --- fuzz_ipc_command: IPC JSON command parser fuzzer ---
  executable(
    'fuzz_ipc_command',
    files('fuzz_ipc_command.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [],
    install: false,
  )

  # --- fuzz_rules: window rules/apps file parser fuzzer ---
  executable(
    'fuzz_rules',
    files('fuzz_rules.c', '../../src/rules.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [wayland_server, wlroots],
    install: false,
  )

  # --- fuzz_mousebind: mouse binding config parser fuzzer ---
  executable(
    'fuzz_mousebind',
    files('fuzz_mousebind.c', '../../src/mousebind.c'),
    include_directories: fuzz_inc,
    c_args: fuzz_args,
    link_args: fuzz_link_args,
    dependencies: [wayland_server, wlroots, xkbcommon],
    install: false,
  )
else
  message('Fuzz targets disabled: compiler does not support -fsanitize=fuzzer')
endif

# Install seed corpus files regardless of fuzzer support
# (useful for manual testing or CI with external fuzzers)
