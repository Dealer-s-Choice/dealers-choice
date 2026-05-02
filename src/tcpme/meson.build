cc = meson.get_compiler('c')

tcpme_win_deps = []
if host_machine.system() == 'windows'
  tcpme_win_deps += cc.find_library('ws2_32', required: true)
endif

tcpme_lib = static_library(
  'tcpme',
  'tcpme.c',
  dependencies: tcpme_win_deps,
  install: false,
)

tcpme_dep = declare_dependency(
  link_with: tcpme_lib,
  include_directories: include_directories('.'),
  dependencies: tcpme_win_deps,
)

subdir('tests')
