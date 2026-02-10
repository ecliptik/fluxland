project(
  'wm-wayland',
  'c',
  version: '0.1.0-dev',
  license: 'MIT',
  meson_version: '>=0.60.0',
  default_options: [
    'c_std=c17',
    'warning_level=3',
  ],
)

add_project_arguments(
  ['-DWLR_USE_UNSTABLE'],
  language: 'c',
)

cc = meson.get_compiler('c')

add_project_arguments(cc.get_supported_arguments([
  '-Wundef',
  '-Wmissing-include-dirs',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Winit-self',
  '-Wstrict-prototypes',
  '-Wimplicit-fallthrough=2',
  '-Wendif-labels',
  '-Wstrict-aliasing=2',
  '-Woverflow',
  '-Wmissing-prototypes',
  '-Wshadow',
  '-Wno-unused-parameter',
]), language: 'c')

version = '"@0@"'.format(meson.project_version())
git = find_program('git', native: true, required: false)
if git.found()
  git_commit = run_command([git, 'describe', '--dirty'], check: false)
  if git_commit.returncode() == 0
    version = '"@0@"'.format(git_commit.stdout().strip())
  endif
endif
add_project_arguments('-DWM_WAYLAND_VERSION=@0@'.format(version), language: 'c')

# --- Core dependencies ---

wlroots = dependency(
  'wlroots-0.18',
  version: ['>=0.18.0', '<0.19.0'],
)

wayland_server = dependency('wayland-server', version: '>=1.22.0')
wayland_protos = dependency('wayland-protocols', version: '>=1.32')
xkbcommon = dependency('xkbcommon')
input = dependency('libinput', version: '>=1.14')
pixman = dependency('pixman-1')
pangocairo = dependency('pangocairo')
libdrm = dependency('libdrm')
math = cc.find_library('m')

# --- Optional dependencies ---

xwayland_opt = get_option('xwayland')
have_xwayland = false
if not xwayland_opt.disabled()
  # Check that wlroots was built with xwayland support
  have_xwayland = cc.has_header('wlr/xwayland/xwayland.h', dependencies: wlroots)
  if not have_xwayland and xwayland_opt.enabled()
    error('XWayland support requested but wlroots was not built with xwayland')
  endif
endif

if have_xwayland
  xcb = dependency('xcb')
  xcb_ewmh = dependency('xcb-ewmh')
  xcb_icccm = dependency('xcb-icccm')
  add_project_arguments('-DWM_HAS_XWAYLAND', language: 'c')
endif

conf_data = configuration_data()
conf_data.set10('HAVE_XWAYLAND', have_xwayland)

configure_file(
  output: 'config.h',
  configuration: conf_data,
)

# --- Sanitizer options ---

if get_option('asan')
  add_project_arguments(['-fsanitize=address', '-fno-omit-frame-pointer'], language: 'c')
  add_project_link_arguments(['-fsanitize=address'], language: 'c')
endif

if get_option('ubsan')
  add_project_arguments(['-fsanitize=undefined'], language: 'c')
  add_project_link_arguments(['-fsanitize=undefined'], language: 'c')
endif

# --- Protocol generation ---

subdir('protocols')

# --- Include directories ---

wm_inc = include_directories('include', 'src')

# --- Collect sources ---

wm_deps = [
  server_protos,
  wayland_server,
  wlroots,
  xkbcommon,
  input,
  pixman,
  pangocairo,
  libdrm,
  math,
]

wm_sources = files(
  'src/main.c',
  'src/server.c',
  'src/output.c',
  'src/input.c',
  'src/keyboard.c',
  'src/cursor.c',
  'src/view.c',
  'src/workspace.c',
  'src/config.c',
  'src/rcparser.c',
  'src/keybind.c',
  'src/mousebind.c',
  'src/style.c',
  'src/render.c',
  'src/decoration.c',
  'src/tabgroup.c',
  'src/ipc.c',
  'src/ipc_commands.c',
  'src/rules.c',
  'src/placement.c',
  'src/layer_shell.c',
  'src/screencopy.c',
  'src/session_lock.c',
  'src/menu.c',
  'src/toolbar.c',
  'src/xwayland.c',
)

if have_xwayland
  wm_deps += [xcb, xcb_ewmh, xcb_icccm]
endif

# --- Build executable ---

executable(
  meson.project_name(),
  wm_sources,
  include_directories: [wm_inc],
  dependencies: wm_deps,
  install: true,
)

# --- IPC client tool ---

executable(
  'wm-wayland-ctl',
  files('tools/wm-wayland-ctl.c'),
  install: true,
)

# --- Tests ---

subdir('tests')
