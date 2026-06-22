project(
  'pcg',
  'c',
  version: '0.94',
  license: 'Apache-2.0 OR MIT',
  # warning_level 1 keeps the vendored upstream code off DC's strict level-3
  # flags. (As a subproject it already builds with its own options and no
  # -Werror, independent of the superproject, but pin it explicitly.)
  default_options: ['warning_level=1'],
)

# Minimal bundled PCG PRNG (pcg_basic), the fallback used when no system pcg-c
# is installed. deckhandler.h includes <pcg_basic.h> in this case (the system
# full library uses <pcg_variants.h> under -DHAVE_PCG instead).
_pcg = static_library(
  'pcg',
  'pcg_basic.c',
)

pcg_dep = declare_dependency(
  link_with: _pcg,
  include_directories: '.',
)

# Let the superproject resolve this via dependency('pcg-c', fallback: 'pcg')
# too, not only subproject('pcg').get_variable('pcg_dep').
meson.override_dependency('pcg-c', pcg_dep)
