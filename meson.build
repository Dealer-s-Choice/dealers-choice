project(
  'dealers-choice',
  ['c'],
  version: '0.0.3.555',
  meson_version: '>= 0.61.0',
  default_options: [
    'warning_level=3',
    'b_sanitize=address,undefined',
    'b_lundef=false',
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
  w_lib = ['ws2_32', 'user32']
  foreach lib : w_lib
    win_deps += cc.find_library(lib, required: true)
  endforeach
endif

# definitions = ['-D_DEFAULT_SOURCE']
# add_project_arguments(definitions, language: 'c')

extra_flags = [
  '-fno-common',
  '-fstack-protector-strong',
  '-Wformat-security',
  '-Wshadow',
  '-Wstrict-overflow=5',
  '-Werror=odr',
  '-Werror=lto-type-mismatch',
  '-Werror=strict-aliasing',
]

# To enable PATH_MAX and friends on Alpine Linux
definitions = [
  '-D_XOPEN_SOURCE'
]

add_project_arguments(cc.get_supported_arguments(extra_flags), language: 'c')
add_project_arguments(definitions, language: 'c')

canfigger_dep = dependency(
  'canfigger',
  default_options: 'default_library=static',
)

deckhandler_dep = dependency(
  'deckhandler',
  default_options: 'default_library=static',
)

pokeval_dep = dependency('pokeval', default_options: 'default_library=static')
protobuf_dep = dependency('libprotobuf-c')

cmake = import('cmake')
sub_proj = cmake.subproject('miniaudio')

# Fetch the dependency object
miniaudio_dep = sub_proj.dependency('miniaudio')

shared_dep = [
  canfigger_dep,
  deckhandler_dep,
  miniaudio_dep,
  pokeval_dep,
  win_deps,
  cc.find_library('m'),
]

dep_names = [
  'libprotobuf-c',
  'SDL2',
  'SDL2_ttf',
  'SDL2_net',
  'SDL2_image',
]

foreach dep : dep_names
  shared_dep += dependency(dep)
endforeach

shared_inc = [include_directories(['src', 'src/protobuf'])]

conf = configuration_data()
subdir('po')
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
  shared_src,
  dependencies: shared_dep,
  include_directories: shared_inc,
)

game_dep = declare_dependency(
  link_with: _lib,
  dependencies: shared_dep,
  include_directories: [shared_inc, include_directories('.')],
)

exe_target = executable(
  meson.project_name(),
  ['src/main.c'],
  dependencies: game_dep,
  install: true,
)

install_subdir(
  'data',
  install_dir: join_paths(get_option('datadir'), meson.project_name()),
  strip_directory: true,
)

# Check the Dockerfile when updating this
install_data(
  files('LICENSE', 'README.md', 'ReleaseNotes'),
  install_dir: get_option('docdir'),
)

install_data(
  'packaging/dealers-choice.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

icon_sizes = ['128', '64', '32', '16']

foreach size : icon_sizes
  install_data(
    'packaging/icons/dealers-choice_@0@x@0@.png'.format(size),
    rename: 'dealers-choice.png',
    install_dir: join_paths(
      get_option('datadir'),
      'icons',
      'hicolor',
      '@0@x@0@'.format(size),
      'apps'
    ),
  )
endforeach

install_data(
  'packaging/icons/dealers-choice.svg',
  install_dir: join_paths(
    get_option('datadir'),
    'icons',
    'hicolor',
    'scalable',
    'apps'
  ),
)


subdir('tests')
