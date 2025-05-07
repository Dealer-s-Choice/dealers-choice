project(
  'aapokerhands',
  'c',
  version: '0.1.1.555',
  meson_version: '>= 0.51.0',
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
  win_deps += cc.find_library('ws2_32')
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

deckhandler_dep = dependency(
  'deckhandler',
  default_options: 'default_library=static',
)

pokeval_dep = dependency(
  'pokeval',
  default_options: 'default_library=static',
)

protobuf_dep = dependency('libprotobuf-c')

net_shared_dep = [pokeval_dep, win_deps]
dep_names = [
  'libprotobuf-c',
  'SDL2',
  'SDL2_ttf',
  'SDL2_net',
  'SDL2_gfx',
  # 'SDL2_mixer',
  ]

foreach dep : dep_names
  net_shared_dep += dependency(dep)
endforeach

net_shared_inc = include_directories(['src', 'src/protobuf'])

subdir('src')

_lib = static_library(
  'game',
  net_shared_src,
  dependencies: net_shared_dep,
  include_directories: net_shared_inc,
  )

game_dep = declare_dependency(
  link_with: _lib,
  dependencies: net_shared_dep,
  include_directories: net_shared_inc,
  )

executable(
  'dealers_choice',
  ['src/main.c',],
  dependencies: game_dep,
  install: false,
)

subdir('tests')
