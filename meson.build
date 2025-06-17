project(
  'dealers-choice',
  'c',
  version: '0.0.1.555',
  meson_version: '>= 0.56.0',
  default_options: [
    'warning_level=3',
    'b_sanitize=address,undefined',
    'b_lundef=false',
    'c_std=c11',
  ],
)

cc = meson.get_compiler('c')

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

add_project_arguments(cc.get_supported_arguments(extra_flags), language: 'c')

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

shared_dep = [
  canfigger_dep,
  deckhandler_dep,
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
  # 'SDL2_mixer',
]

foreach dep : dep_names
  shared_dep += dependency(dep)
endforeach

shared_inc = include_directories(['src', 'src/protobuf'])

subdir('src')

_lib = static_library(
  'game',
  shared_src,
  dependencies: shared_dep,
  include_directories: shared_inc,
)

game_dep = declare_dependency(
  link_with: _lib,
  dependencies: shared_dep,
  include_directories: shared_inc,
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

install_data(
  'packaging/dealers-choice_icon_32x32.png',
  rename: 'dealers-choice.png',
  install_dir: join_paths(
    get_option('datadir'),
    'icons',
    'hicolor',
    '32x32',
    'apps',
  ),
)

subdir('tests')
