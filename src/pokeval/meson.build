libname = 'pokeval'

buildtarget = static_library(
  libname,
  'pokeval.c',
  dependencies: deckhandler_dep,
)

pokeval_dep = declare_dependency(
  link_with: buildtarget,
  dependencies: deckhandler_dep,
  include_directories: '.',
)

subdir('tests')
