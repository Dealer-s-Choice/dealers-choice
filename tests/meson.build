unit_tests = ['serialization', 'get_next_player', 'debug_print_cards', 'layout_cards']

base_env = {
  'DEALERSCHOICE_DATADIR': join_paths(meson.project_source_root(), 'data'),
  'MESON_BUILD_ROOT': meson.project_build_root(),
  'MESON_BUILD_TEST_ROOT': meson.current_build_dir(),
}

foreach name : unit_tests
  exe = executable(
    'test_' + name,
    [name + '.c', '00_test.c'],
    dependencies: game_dep,
    override_options: ['b_lto=false'],
  )
  test('test_' + name, exe, env: environment(base_env))
endforeach

# Each game logic test gets a unique port so they can run in parallel.
game_logic_script = files(join_paths(meson.current_source_dir(), 'game_logic.py'))

game_logic = [
  ['fold',            '22778'],
  ['check',           '22779'],
  ['5_card_showdown', '22780'],
  ['5_card_draw',     '22781'],
  ['5_card_stud',     '22782'],
  ['deuces_wild',     '22783'],
]

foreach entry : game_logic
  name = entry[0]
  port = entry[1]

  exe = executable(
    'test_' + name,
    [name + '.c', '00_test.c'],
    dependencies: game_dep,
    override_options: ['b_lto=false'],
  )

  env = environment(base_env)
  env.set('DC_PORT', port)

  test(
    'test_' + name,
    game_logic_script,
    args: ['test_' + name],
    env: env,
    timeout: 90,
    depends: [_lib, exe_target],
  )
endforeach

add_test_setup(
  'One_pass',
  #exe_wrapper: [join_paths(meson.current_source_dir(), '5_card_showdown.sh')],
  env: environment(base_env + {'DC_NPASSES': '1'}),
)
