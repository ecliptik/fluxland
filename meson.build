project(
  'fluxland',
  'c',
  version: '1.0.0',
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
  # Security hardening
  '-fstack-protector-strong',
  '-fstack-clash-protection',
  '-Wformat',
  '-Wformat-security',
]), language: 'c')

# Security hardening: FORTIFY_SOURCE requires optimization
if get_option('buildtype') != 'debug'
  add_project_arguments(cc.get_supported_arguments([
    '-D_FORTIFY_SOURCE=2',
  ]), language: 'c')
endif

add_project_link_arguments(cc.get_supported_link_arguments([
  '-Wl,-z,relro',
  '-Wl,-z,now',
]), language: 'c')

version = '"@0@"'.format(meson.project_version())
git = find_program('git', native: true, required: false)
if git.found()
  git_commit = run_command([git, 'describe', '--dirty'], check: false)
  if git_commit.returncode() == 0
    version = '"@0@"'.format(git_commit.stdout().strip())
  endif
endif
add_project_arguments('-DFLUXLAND_VERSION=@0@'.format(version), language: 'c')

# --- Gettext / i18n ---

intl_dep = dependency('intl', required: false)
have_gettext = intl_dep.found() or cc.has_function('gettext', prefix: '#include <libintl.h>')
if have_gettext
  add_project_arguments('-DHAVE_GETTEXT', language: 'c')
  localedir = get_option('prefix') / get_option('localedir')
  add_project_arguments('-DLOCALEDIR="@0@"'.format(localedir), language: 'c')
endif

# --- Gettext catalog generation ---

if have_gettext
  subdir('po')
endif

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

# System tray (StatusNotifierItem via sd-bus)
sdbus_dep = dependency('libsystemd', required: false)
if not sdbus_dep.found()
  sdbus_dep = dependency('libelogind', required: false)
endif
have_systray = sdbus_dep.found()
if have_systray
  add_project_arguments('-DWM_HAS_SYSTRAY', language: 'c')
endif

# AT-SPI accessibility bridge (optional, uses sd-bus)
have_atspi = false
if get_option('atspi')
  if sdbus_dep.found()
    have_atspi = true
    add_project_arguments('-DWM_HAS_ATSPI', language: 'c')
  else
    error('AT-SPI bridge requested but no sd-bus library found')
  endif
endif

# PipeWire screen recording
pipewire_opt = get_option('pipewire')
pipewire_dep = dependency('libpipewire-0.3', version: '>=0.3.0', required: pipewire_opt)
have_pipewire = pipewire_dep.found()
if have_pipewire
  add_project_arguments('-DWM_HAS_PIPEWIRE', language: 'c')
endif

conf_data = configuration_data()
conf_data.set10('HAVE_XWAYLAND', have_xwayland)
conf_data.set10('WM_HAS_SYSTRAY', have_systray)
conf_data.set10('WM_HAS_ATSPI', have_atspi)
conf_data.set10('WM_HAS_PIPEWIRE', have_pipewire)

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

if get_option('perf')
  add_project_arguments('-DWM_PERF_ENABLE', language: 'c')
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

if have_gettext and intl_dep.found()
  wm_deps += [intl_dep]
endif

wm_sources = files(
  'src/main.c',
  'src/animation.c',
  'src/atspi_bridge.c',
  'src/autostart.c',
  'src/server.c',
  'src/output.c',
  'src/input.c',
  'src/keyboard.c',
  'src/keyboard_actions.c',
  'src/cursor.c',
  'src/cursor_actions.c',
  'src/cursor_snap.c',
  'src/view.c',
  'src/view_focus.c',
  'src/view_geometry.c',
  'src/workspace.c',
  'src/config.c',
  'src/rcparser.c',
  'src/keybind.c',
  'src/mousebind.c',
  'src/style.c',
  'src/render.c',
  'src/decoration.c',
  'src/focus_nav.c',
  'src/fractional_scale.c',
  'src/gamma_control.c',
  'src/foreign_toplevel.c',
  'src/tabgroup.c',
  'src/ipc.c',
  'src/ipc_commands.c',
  'src/rules.c',
  'src/placement.c',
  'src/idle.c',
  'src/layer_shell.c',
  'src/output_management.c',
  'src/protocols.c',
  'src/presentation.c',
  'src/screencopy.c',
  'src/screen_cast.c',
  'src/session_lock.c',
  'src/text_input.c',
  'src/transient_seat.c',
  'src/drm_lease.c',
  'src/drm_syncobj.c',
  'src/viewporter.c',
  'src/menu.c',
  'src/menu_parse.c',
  'src/menu_render.c',
  'src/slit.c',
  'src/tablet.c',
  'src/toolbar.c',
  'src/util.c',
  'src/perf.c',
  'src/validate.c',
  'src/wallpaper.c',
  'src/xwayland.c',
)

if have_xwayland
  wm_deps += [xcb, xcb_ewmh, xcb_icccm]
endif

if have_systray
  wm_sources += files('src/systray.c')
  wm_deps += [sdbus_dep]
elif have_atspi
  # AT-SPI uses sd-bus but systray already added it if both are enabled
  wm_deps += [sdbus_dep]
endif

if have_pipewire
  wm_deps += [pipewire_dep]
endif

# --- Build executable ---

fluxland_exe = executable(
  meson.project_name(),
  wm_sources,
  include_directories: [wm_inc],
  dependencies: wm_deps,
  install: true,
)

# --- WLCS integration module (optional) ---

if get_option('wlcs')
  # All compositor sources except main.c (rebuilt for shared library)
  wlcs_compositor_sources = files(
    'src/animation.c',
    'src/atspi_bridge.c',
    'src/autostart.c',
    'src/server.c',
    'src/output.c',
    'src/input.c',
    'src/keyboard.c',
    'src/keyboard_actions.c',
    'src/cursor.c',
    'src/cursor_actions.c',
    'src/cursor_snap.c',
    'src/view.c',
    'src/view_focus.c',
    'src/view_geometry.c',
    'src/workspace.c',
    'src/config.c',
    'src/rcparser.c',
    'src/keybind.c',
    'src/mousebind.c',
    'src/style.c',
    'src/render.c',
    'src/decoration.c',
    'src/focus_nav.c',
    'src/fractional_scale.c',
    'src/gamma_control.c',
    'src/foreign_toplevel.c',
    'src/tabgroup.c',
    'src/ipc.c',
    'src/ipc_commands.c',
    'src/rules.c',
    'src/placement.c',
    'src/idle.c',
    'src/layer_shell.c',
    'src/output_management.c',
    'src/protocols.c',
    'src/presentation.c',
    'src/screencopy.c',
    'src/screen_cast.c',
    'src/session_lock.c',
    'src/text_input.c',
    'src/transient_seat.c',
    'src/drm_lease.c',
    'src/drm_syncobj.c',
    'src/viewporter.c',
    'src/menu.c',
    'src/menu_parse.c',
    'src/menu_render.c',
    'src/slit.c',
    'src/tablet.c',
    'src/toolbar.c',
    'src/util.c',
    'src/perf.c',
    'src/validate.c',
    'src/wallpaper.c',
    'src/xwayland.c',
  )

  if have_systray
    wlcs_compositor_sources += files('src/systray.c')
  endif

  threads = dependency('threads')
  wayland_client_dep = dependency('wayland-client')

  shared_library(
    'fluxland-wlcs',
    wlcs_compositor_sources + files('src/wlcs_shim.c'),
    include_directories: [wm_inc],
    dependencies: wm_deps + [threads, wayland_client_dep],
    gnu_symbol_visibility: 'hidden',
    install: false,
  )
endif

# --- IPC client tool ---

executable(
  'fluxland-ctl',
  files('tools/fluxland-ctl.c'),
  install: true,
)

# --- Install data files ---

install_data(
  'data/fluxland.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data(
  'data/fluxland-session.desktop',
  install_dir: get_option('datadir') / 'wayland-sessions',
)

example_configs = [
  'data/apps',
  'data/init',
  'data/keys',
  'data/menu',
  'data/startup',
  'data/style',
  'data/fluxland-hc-dark.style',
  'data/fluxland-hc-light.style',
  'data/fluxland-wave.style',
]

install_data(
  example_configs,
  install_dir: get_option('datadir') / 'fluxland' / 'examples',
)

# --- Man pages ---

install_man(
  'man/fluxland.1',
  'man/fluxland-ctl.1',
  'man/fluxland-keys.5',
  'man/fluxland-apps.5',
  'man/fluxland-init.5',
  'man/fluxland-menu.5',
  'man/fluxland-startup.5',
  'man/fluxland-style.5',
)

# --- Tests ---

subdir('tests')

# --- Linting targets ---

clang_format = find_program('clang-format', required: false, disabler: true)
clang_tidy_prog = find_program('clang-tidy', required: false, disabler: true)

lint_src_dirs = [
  meson.project_source_root() / 'src',
  meson.project_source_root() / 'tools',
  meson.project_source_root() / 'tests',
]

lint_c = run_command('find', lint_src_dirs, '-name', '*.c', check: true,
).stdout().strip().split('\n')

lint_h = run_command('find', lint_src_dirs, '-name', '*.h', check: true,
).stdout().strip().split('\n')

run_target('clang-format',
  command: [clang_format, '-i'] + lint_c + lint_h,
)

run_target('clang-format-check',
  command: [clang_format, '--dry-run', '--Werror'] + lint_c + lint_h,
)

# Exclude files not in compile_commands.json (fuzz targets, wlcs shim)
tidy_c = run_command('find', lint_src_dirs,
  '-name', '*.c',
  '-not', '-path', '*/fuzz/*',
  '-not', '-name', 'wlcs_shim.c',
  check: true,
).stdout().strip().split('\n')

run_target('clang-tidy',
  command: [clang_tidy_prog, '-p', meson.project_build_root()] + tidy_c,
)
