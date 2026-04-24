project(
    'pcg',
    'c',
    version: '0.9',
    meson_version: '>= 0.56.0',
    default_options: ['warning_level=3', 'c_std=c99'],
)

cc = meson.get_compiler('c')

if cc.get_id() != 'msvc'
  extra_flags = [
      '-fno-common',
      '-fstack-protector-strong',
      '-Wformat-security',
      '-Wshadow',
      '-Wstrict-overflow=5',
      '-Werror=odr',
      '-Werror=strict-aliasing',
  ]
  add_project_arguments(cc.get_supported_arguments(extra_flags), language: 'c')
endif

_pcg = static_library('pcg', 'pcg_basic.c')

pcg_dep = declare_dependency(link_with: _pcg, include_directories: '.')
