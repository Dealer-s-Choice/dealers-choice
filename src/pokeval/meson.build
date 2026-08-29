pokeval_lib = static_library(
  'pokeval',
  'pokeval.c',
  dependencies: deckhandler_dep,
)

pokeval_dep = declare_dependency(
  link_with: pokeval_lib,
  dependencies: deckhandler_dep,
  include_directories: '.',
)

subdir('tests')
