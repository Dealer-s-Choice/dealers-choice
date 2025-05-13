cases = [
  'serialization',
  'create_player_list',
]

foreach test : cases

  exe = executable(
    'test_' + test,
    test + '.c',
    c_args: ['-DTEST_LIB'],
    dependencies: game_dep,
    override_options: ['b_lto=false'],
  )
  test('test_' + test, exe)
endforeach
