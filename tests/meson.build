cases = ['serialization', 'get_next_player', 'debug_print_cards']

env = environment()
env.set('DEALERSCHOICE_DATADIR', join_paths(meson.project_source_root(), 'data'))
foreach test : cases

  exe = executable(
    'test_' + test,
    [test + '.c', '00_test.c'],
    dependencies: game_dep,
    override_options: ['b_lto=false'],
  )
  test('test_' + test,
    exe,
    env: env,
    )
endforeach
