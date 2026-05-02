
src = files('deckhandler.c')

pcg_dep = dependency('pcg-c', required: false)
have_system_pcg = pcg_dep.found()

if not have_system_pcg
  subdir('pcg')
endif

libname = 'deckhandler'

deckhandler_c_args = []
if have_system_pcg
  deckhandler_c_args += '-DHAVE_PCG'
endif

buildtarget = static_library(
  libname,
  src,
  c_args: deckhandler_c_args,
  dependencies: pcg_dep,
)

deckhandler_dep = declare_dependency(
  link_with: buildtarget,
  compile_args: deckhandler_c_args,
  dependencies: pcg_dep,
  include_directories: '.',
)

subdir('test')
