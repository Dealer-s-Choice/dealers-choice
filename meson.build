project(
  'dealers-choice',
  ['c'],
  version: '0.0.13',
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

miniaudio_dep = subproject('miniaudio').get_variable('miniaudio_dep')

shared_dep = [
  canfigger_dep,
  miniaudio_dep,
  win_deps,
]

if not platform_is_windows
  shared_dep += cc.find_library('m')
endif

sodium_dep = dependency('libsodium', required: true)
shared_dep += sodium_dep

dep_names = [
  'libprotobuf-c',
  'SDL2',
]

# SDL2_ttf and SDL2_image are only used by the client (rendering); the bot
# must not link against them or namcap/lintian will flag them as unused.
client_only_dep_names = [
  'SDL2_ttf',
  'SDL2_image',
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

shared_inc = [include_directories(['src', 'src/protobuf'])]

subdir('po')

subdir('src/tcpme')
shared_dep += tcpme_dep

# SDL3_net ships as SDL3_net::SDL3_net-static in vcpkg static builds; specify
# the CMake target explicitly so Meson doesn't fall back to the imprecise
# SDL3_net_LIBRARIES variable when pkgconfig is unavailable (e.g. MSVC+vcpkg).
sdl3_net_dep = dependency('SDL3_net', modules: ['SDL3_net::SDL3_net-static'], required: false)
if not sdl3_net_dep.found()
  cmake = import('cmake')
  _cmake_build_type = {
    'plain':          'Release',
    'debug':          'Debug',
    'debugoptimized': 'RelWithDebInfo',
    'release':        'Release',
    'minsize':        'MinSizeRel',
  }.get(get_option('buildtype'), 'RelWithDebInfo')
  sdl3_net_opts = cmake.subproject_options()
  sdl3_net_opts.add_cmake_defines({
    'BUILD_SHARED_LIBS':                  'OFF',
    'SDLNET_INSTALL':                     'OFF',
    'SDLNET_SAMPLES':                     'OFF',
    'CMAKE_BUILD_TYPE':                   _cmake_build_type,
    'CMAKE_SUPPRESS_DEVELOPER_WARNINGS':  'ON',
  })
  # Meson injects -std=c11 and -Wpedantic from the parent project. Counter with
  # -std=gnu99 (matching SDL3_net's own CMake build) to expose POSIX networking
  # symbols, and -Wno-pedantic to allow the function→void* cast in SDL_net.c.
  sdl3_net_opts.append_compile_args('c', '-std=gnu99', '-Wno-pedantic')
  sdl3_net_sub = cmake.subproject('sdl3_net', options: sdl3_net_opts)
  sdl3_net_dep = sdl3_net_sub.dependency('SDL3_net-static')
endif
shared_dep += sdl3_net_dep

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

subdir('src')

configure_file(output: 'config.h', configuration: conf)

_lib = static_library(
  'game',
  [shared_src, widgets_src],
  dependencies: shared_dep + client_only_deps,
  include_directories: shared_inc,
)

game_dep = declare_dependency(
  link_with: _lib,
  dependencies: shared_dep + client_only_deps,
  include_directories: [shared_inc, include_directories('.')],
)

_bot_lib = static_library(
  'game_bot',
  bot_src,
  dependencies: shared_dep,
  include_directories: shared_inc,
)

bot_dep = declare_dependency(
  link_with: _bot_lib,
  dependencies: shared_dep,
  include_directories: [shared_inc, include_directories('.')],
)

exe_target = executable(
  meson.project_name(),
  ['src/main.c'],
  dependencies: game_dep,
  install: true,
)

executable(
  meson.project_name() + '-bot',
  ['src/bot.c'],
  dependencies: bot_dep,
  install: true,
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
