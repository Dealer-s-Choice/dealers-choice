
deckhandler_src = files('deckhandler.c')

# pcg_dep is resolved once at the top level (system pcg-c or the bundled
# subprojects/pcg fallback) and already carries -DHAVE_PCG when the system lib
# is used, so deckhandler.h picks the right header here and in every consumer.
deckhandler_lib = static_library(
  'deckhandler',
  deckhandler_src,
  dependencies: pcg_dep,
)

deckhandler_dep = declare_dependency(
  link_with: deckhandler_lib,
  dependencies: pcg_dep,
  include_directories: '.',
)

subdir('test')
