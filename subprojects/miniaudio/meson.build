project('miniaudio', 'c')

# Compile without parent project's POSIX feature-test flags so that
# BSD-only types (u_long etc.) remain visible on FreeBSD.
_ma_lib = static_library(
  'miniaudio',
  'miniaudio.c',
  include_directories: include_directories('miniaudio'),
  c_args: ['-U_POSIX_C_SOURCE', '-U_XOPEN_SOURCE', '-Wno-strict-overflow'],
  override_options: ['b_sanitize=none', 'b_lto=false', 'warning_level=0'],
)

miniaudio_dep = declare_dependency(
  link_with: _ma_lib,
  include_directories: include_directories('.'),
)
