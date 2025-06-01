cases = ['serialization', 'get_next_player', 'debug_print_cards']

env_dict = {
  'DEALERSCHOICE_DATADIR': join_paths(meson.project_source_root(), 'data'),
  'MESON_BUILD_ROOT': meson.project_build_root(),
  'MESON_BUILD_TEST_ROOT': meson.current_build_dir(),
}

foreach test : cases
  exe = executable(
    'test_' + test,
    [test + '.c', '00_test.c'],
    dependencies: game_dep,
    override_options: ['b_lto=false'],
  )
  test('test_' + test, exe, env: environment(env_dict))
endforeach

game_logic = ['5_card_showdown', '5_card_stud']

foreach test : game_logic
  # This is used by script, so not tested with the others above
  exe = executable(
    'test_' + test,
    [test + '.c', '00_test.c'],
    dependencies: game_dep,
    override_options: ['b_lto=false'],
  )

  test(
    'test_' + test,
    files(join_paths(meson.current_source_dir(), 'game_logic.py')),
    args: ['test_' + test],
    env: environment(env_dict),
    is_parallel: false,
    timeout: 120,
    depends: [_lib, exe_target],
  )
endforeach

add_test_setup(
  'One_pass',
  #exe_wrapper: [join_paths(meson.current_source_dir(), '5_card_showdown.sh')],
  env: environment(env_dict + {'DC_NPASSES': '1'}),
)
