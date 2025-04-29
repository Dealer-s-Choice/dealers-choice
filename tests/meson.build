exe = executable(
  'test_lib',
  'serialization.c',
  c_args: ['-DTEST_LIB'],
  dependencies: game_dep,
  override_options: ['b_lto=false'],
)
test('test_serialization', exe)
