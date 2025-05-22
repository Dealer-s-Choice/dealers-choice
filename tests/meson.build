cases = [
  'serialization',
  'get_next_player',
]

foreach test : cases

  exe = executable(
    'test_' + test,
    test + '.c',
    dependencies: game_dep,
    override_options: ['b_lto=false'],
  )
  test('test_' + test, exe)
endforeach
