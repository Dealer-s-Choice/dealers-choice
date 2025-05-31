cases = ['serialization', 'get_next_player', 'debug_print_cards']

env = environment()
env.set('DEALERSCHOICE_DATADIR', join_paths(meson.project_source_root(), 'data'))
env.set('MESON_BUILD_ROOT', meson.project_build_root())
env.set('MESON_BUILD_TEST_ROOT', meson.current_build_dir())

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

# This is used by script, so not tested with the others above
executable(
  'test_5_card_showdown',
  ['5_card_showdown.c', '00_test.c'],
  dependencies: game_dep,
  override_options: ['b_lto=false'],
)

test(
  'test_5_card_showdown',
  files(join_paths(meson.current_source_dir(), '5_card_showdown.sh')),
  env: env,
  timeout: 60,
  )
  
 # This is used by script, so not tested with the others above
executable(
  'test_5_card_stud',
  ['5_card_stud.c', '00_test.c'],
  dependencies: game_dep,
  override_options: ['b_lto=false'],
)

test(
  'test_5_card_stud',
  files(join_paths(meson.current_source_dir(), '5_card_stud.sh')),
  env: env,
  timeout: 120,
  )

#add_test_setup(
#    'test_5_card_show_down',
#    exe_wrapper: [join_paths(meson.current_source_dir(), '5_card_showdown.sh')],
#    env: env
#)
