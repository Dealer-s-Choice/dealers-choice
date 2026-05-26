
unit_tests = [
  'serialization',
  'get_next_player',
  'debug_print_cards',
  'layout_cards',
  'no_peek',
  'rate_limit',
  'card_texture_cache',
]

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

# All tests that bind a TCP socket get a unique port derived from base_port so
# they can run in parallel without colliding.  Add new entries to game_logic
# below and they will receive the next port automatically.
base_port = 22778

sodium_compat_exe = executable(
  'test_sodium_compat',
  ['sodium_compat.c', '00_test.c'],
  dependencies: game_dep,
  override_options: ['b_lto=false'],
)
sodium_compat_env = environment(base_env)
sodium_compat_env.set('DC_PORT', base_port.to_string())
test('test_sodium_compat', sodium_compat_exe, env: sodium_compat_env)

game_logic_script = files(
  join_paths(meson.current_source_dir(), 'game_logic.py'),
)

game_logic = [
  'fold',
  'check',
  '5_card_showdown',
  '5_card_draw',
  '5_card_stud',
  'kick_ban',
  'texas_holdem',
  'raises',
]

port = base_port + 1
foreach name : game_logic
  exe = executable(
    'test_' + name,
    [name + '.c', '00_test.c'],
    dependencies: game_dep,
    override_options: ['b_lto=false'],
  )

  env = environment(base_env)
  env.set('DC_PORT', port.to_string())

  test(
    'test_' + name,
    game_logic_script,
    args: ['test_' + name],
    env: env,
    timeout: 90,
    depends: [_lib, exe_target],
  )

  port += 1
endforeach

pokeval_fuzz_exe = executable(
  'test_pokeval_fuzz',
  'pokeval_fuzz.c',
  dependencies: game_dep,
  override_options: ['b_lto=false'],
)

# Run the fuzzer and cross-check pokeval's verdicts against the
# Python analyzer.  1000 hands runs in <1s sanitized locally; budget
# 180s to cover s390x-under-QEMU (OpenSUSE Tumbleweed CI) and the
# OpenBSD VM runner, both of which are ~50x slower than native.
# Override hand count with the DC_FUZZ_N env var, seed with
# DC_FUZZ_SEED.
pokeval_fuzz_check = files(
  join_paths(meson.current_source_dir(), 'pokeval_fuzz_check.py'),
)
test(
  'test_pokeval_fuzz',
  pokeval_fuzz_check,
  env: environment(base_env + {
    'MESON_SOURCE_ROOT': meson.project_source_root(),
  }),
  timeout: 180,
  depends: [pokeval_fuzz_exe],
)

add_test_setup(
  'One_pass',
  #exe_wrapper: [join_paths(meson.current_source_dir(), '5_card_showdown.sh')],
  env: environment(base_env + {'DC_NPASSES': '1'}),
)
