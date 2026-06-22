project(
  'dealers-choice',
  ['c'],
  version: '0.0.16',
  meson_version: '>= 0.61.0',
  default_options: [
    'warning_level=3',
    'c_std=c11',
  ],
)

cc = meson.get_compiler('c')

# Sets the localedir to the location of the po files in the repo after using
# 'meson devenv' so they can be tested without installing
meson.add_devenv(
  {'DEALERSCHOICE_LOCALEDIR': join_paths(meson.project_build_root(), 'po')},
)

host_sys = host_machine.system()
platform_is_windows = (host_sys == 'windows')

win_deps = []
if platform_is_windows
  w_lib = ['user32', 'shell32']
  foreach lib : w_lib
    win_deps += cc.find_library(lib, required: true)
  endforeach
endif

# definitions = ['-D_DEFAULT_SOURCE']
# add_project_arguments(definitions, language: 'c')

if cc.get_id() != 'msvc'
  extra_flags = [
    '-fno-common',
    '-fstack-protector-strong',
    '-Wformat-security',
    '-Wshadow',
    '-Wstrict-overflow=2',
    '-Werror=odr',
    '-Werror=lto-type-mismatch',
    '-Werror=strict-aliasing',
    '-Werror=int-conversion',
  ]
  add_project_arguments(cc.get_supported_arguments(extra_flags), language: 'c')

  if host_sys == 'linux'
    # To enable PATH_MAX and friends on Alpine Linux
    definitions = [
      '-D_XOPEN_SOURCE',
      '-D_POSIX_C_SOURCE=200112L',  # enable setenv() on Ubuntu Jammy
    ]
    add_project_arguments(definitions, language: 'c')
  endif
else
  add_project_arguments(
    [
      '-D_CRT_SECURE_NO_WARNINGS',
      # SDL redefines main→SDL_main on Windows; this prevents that for all TUs
      '-DSDL_MAIN_HANDLED',
      # protobuf-c.h defaults to __declspec(dllimport) on MSVC; override to
      # nothing so the static lib symbol is resolved as a regular extern
      '-DPROTOBUF_C__API=',
      # Required for <stdatomic.h> under /std:c11
      '/experimental:c11atomics',
    ],
    language: 'c',
  )
endif

canfigger_dep = dependency(
  'canfigger',
  version: '>=0.3.1',
  fallback: ['canfigger', 'canfigger_dep'],
  default_options: ['default_library=static'],
)

protobuf_dep = dependency('libprotobuf-c')

conf = configuration_data()

miniaudio_dep = dependency(
  'miniaudio',
  version: '0.11.25',
  required: false,
  fallback: ['miniaudio', 'miniaudio_dep'],
)
#miniaudio_dep = subproject('miniaudio').get_variable('miniaudio_dep')

shared_dep = [
  canfigger_dep,
  win_deps,
]

if not platform_is_windows
  shared_dep += cc.find_library('m')
endif

sodium_dep = dependency('libsodium', required: true)
shared_dep += sodium_dep

dep_names = [
  'libprotobuf-c',
]

# SDL2_ttf and SDL2_image are only used by the client (rendering); the bot
# must not link against them or namcap/lintian will flag them as unused.
# SDL2_image >= 2.0.5 is needed for IMG_LoadSVG_RW (used by the suit-symbol
# atlas in src/widgets/card_text_atlas.c).  2.0.5 is what Ubuntu Jammy ships,
# so this is the practical Linux floor.
client_only_dep_names = ['SDL2_ttf']
client_only_deps_versioned = [
  ['SDL2_image', '>= 2.0.5'],
]

# On Slackware, the miniaudio build fails (linking) if dl and threads aren't added here
# On FreeBSD, threads is needed.
shared_dep += dependency('threads')
if host_sys == 'linux'
  shared_dep += [cc.find_library('dl')]
  dep_names += ['threads']
endif

foreach dep : dep_names
  shared_dep += dependency(dep)
endforeach

client_only_deps = []
foreach dep : client_only_dep_names
  client_only_deps += dependency(dep)
endforeach
foreach pair : client_only_deps_versioned
  client_only_deps += dependency(pair[0], version: pair[1])
endforeach

# GUI-only dependencies: SDL and audio. The headless core library (libdc_core)
# links none of these; only the GUI binary does.
gui_deps = client_only_deps + [dependency('SDL2'), miniaudio_dep]

shared_inc = [
  include_directories(
    ['src', 'src/protobuf', 'src/net', 'src/game', 'src/server', 'src/ui'],
  ),
]

subdir('po')

subdir('src/tcpme')
shared_dep += tcpme_dep

conf.set_quoted(
  'DEALERSCHOICE_LOCALEDIR',
  join_paths(get_option('prefix'), get_option('localedir')),
)
conf.set_quoted('DEALERSCHOICE_VERSION', meson.project_version())
conf.set_quoted('DEALERSCHOICE_FORMAL_NAME', 'Dealer\'s Choice')
conf.set_quoted('DEALERSCHOICE_NAME', meson.project_name())
conf.set_quoted(
  'DEALERSCHOICE_URL',
  'https://dealer-s-choice.github.io/',
)
conf.set_quoted(
  'DEALERSCHOICE_BUGREPORT',
  'https://github.com/Dealer-s-Choice/dealers_choice/issues',
)
conf.set_quoted(
  'DEALERSCHOICE_DATADIR',
  join_paths(get_option('prefix'), get_option('datadir'), meson.project_name()),
)

# pcg-c PRNG: prefer a system install (full pcg-c, <pcg_variants.h>); otherwise
# fall back to the bundled minimal copy in subprojects/pcg (<pcg_basic.h>). The
# -DHAVE_PCG flag is set only for the system lib and selects the header in
# deckhandler.h, so it must reach every translation unit that includes
# deckhandler.h -- it is attached to pcg_dep here and added to shared_dep, which
# makes DC's direct use of pcg explicit instead of leaking transitively through
# deckhandler_dep (#92). deckhandler also lists pcg_dep for its own .c.
pcg_dep = dependency('pcg-c', required: false)
if pcg_dep.found()
  pcg_dep = declare_dependency(
    dependencies: pcg_dep,
    compile_args: ['-DHAVE_PCG'],
  )
else
  pcg_dep = subproject('pcg').get_variable('pcg_dep')
endif
shared_dep += pcg_dep

subdir('src')

configure_file(output: 'config.h', configuration: conf)

# Headless core: game rules, protocol/serialization, server engine. No SDL,
# no audio. Linked by the GUI binary, the bot, and (later) a headless server.
libdc_core = static_library(
  'dc_core',
  common_src,
  dependencies: shared_dep,
  include_directories: shared_inc,
)

dc_core_dep = declare_dependency(
  link_with: libdc_core,
  dependencies: shared_dep,
  include_directories: [shared_inc, include_directories('.')],
)

# GUI library: rendering, widgets, menus. Adds SDL + audio on top of the core.
_lib = static_library(
  'game',
  [gui_src, widgets_src],
  dependencies: [dc_core_dep] + gui_deps,
  include_directories: shared_inc,
)

game_dep = declare_dependency(
  link_with: _lib,
  dependencies: [dc_core_dep] + gui_deps,
  include_directories: [shared_inc, include_directories('.')],
)

exe_sources = ['src/main.c']
if platform_is_windows
  windows = import('windows')
  exe_sources += windows.compile_resources(
    'icons/dealers-choice.rc',
    depend_files: ['icons/dealers-choice.ico'],
  )
endif

exe_target = executable(
  meson.project_name(),
  exe_sources,
  dependencies: game_dep,
  install: true,
)

executable(
  meson.project_name() + '-bot',
  ['src/bot.c'],
  dependencies: dc_core_dep,
  install: true,
)

# Headless server: the same server engine, linking only libdc_core (no
# SDL/audio).  Closes #310/#34.
executable(
  meson.project_name() + '-server',
  ['src/server/server_main.c'],
  dependencies: dc_core_dep,
  install: true,
)

# Headless server registry (directory). Links libdc_core only (no SDL).
executable(
  meson.project_name() + '-registry',
  ['src/registry/registry_main.c'],
  dependencies: dc_core_dep,
  install: true,
)

# Optional diagnostic CLIs (registry-probe, ...). Off by default; -Dtools=true.
subdir('tools')

# Build-time tool that turns the shared hotkey table (src/ui/hotkey_table.c)
# into docs/keys.md.  Built as a native tool so it runs on the build machine
# even when cross-compiling.  It compiles the table directly (no SDL) so the
# generated docs and the in-game F1 overlay always agree on the key list.
gen_keys_doc = executable(
  'gen_keys_doc',
  ['tools/gen_keys_doc.c', 'src/ui/hotkey_table.c'],
  include_directories: include_directories('src', 'src/ui'),
  native: true,
  install: false,
)

# Generate docs/keys.md into the build tree at build time.  This is not installed
# directly: install_subdir('docs') ships the committed docs/keys.md (kept current
# via the regen-keys-doc target below and committed so raw git-tag builds, which
# skip dist processing, still ship it).  Building it here verifies the generator
# compiles and runs on every build, so drift between the table and the docs is
# caught at build time.
keys_doc = custom_target(
  'gen-keys-doc',
  output: 'keys.md',
  command: [gen_keys_doc, '@OUTPUT@'],
  build_by_default: true,
)

# Convenience target to refresh the committed docs/keys.md in the source tree
# (so the version in git stays current): `meson compile -C <build> regen-keys-doc`.
run_target(
  'regen-keys-doc',
  command: [
    gen_keys_doc,
    join_paths(meson.project_source_root(), 'docs', 'keys.md'),
  ],
)

install_subdir(
  'data',
  install_dir: join_paths(get_option('datadir'), meson.project_name()),
  strip_directory: true,
)

install_data(
  files('ChangeLog', 'LICENSE', 'README.md'),
  install_dir: get_option('docdir'),
)

install_subdir('docs', install_dir: get_option('docdir'))

# Optional systemd unit for the headless dealers-choice-server. Left commented
# out on purpose: server hosting is still experimental and lightly tested (see
# docker/README.md). Uncomment to ship it once that changes. The unit directory
# can also be detected via dependency('systemd').get_variable('systemdsystemunitdir').
#install_data(
#  'packaging/systemd/dealers-choice-server.service',
#  install_dir: join_paths(get_option('prefix'), 'lib', 'systemd', 'system'),
#)

# licensedir was added as a core directory option in 1.1.0
# https://mesonbuild.com/Release-notes-for-1-1-0.html#a-new-core-directory-option-licensedir-is-available

# On systems using meson >= 1.1.0, adding it to meson_option.txt results in
# ../meson_options.txt:7:0: ERROR: Option name licensedir is reserved.
#
#license_dir = get_option('licensedir')
#if license_dir == ''
#  license_dir = get_option('docdir')
#endif

#install_data(files('LICENSE'), install_dir: license_dir)

if not platform_is_windows
  install_data(
    'dealers-choice.desktop',
    install_dir: join_paths(get_option('datadir'), 'applications'),
  )

  icon_sizes = ['128', '64', '32', '16']

  foreach size : icon_sizes
    install_data(
      'icons/dealers-choice_@0@x@0@.png'.format(size),
      rename: 'dealers-choice.png',
      install_dir: join_paths(
        get_option('datadir'),
        'icons',
        'hicolor',
        '@0@x@0@'.format(size),
        'apps',
      ),
    )
  endforeach

  install_data(
    'icons/dealers-choice.svg',
    install_dir: join_paths(
      get_option('datadir'),
      'icons',
      'hicolor',
      'scalable',
      'apps',
    ),
  )
endif

# Third-party licenses
install_subdir('licenses', install_dir: get_option('docdir'))

subdir('tests')
