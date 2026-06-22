
unit_tests = [
  'serialization',
  'registry',
  'get_next_player',
  'debug_print_cards',
  'layout_cards',
  'no_peek',
  'rate_limit',
]

base_env = {
  'DEALERSCHOICE_DATADIR': join_paths(meson.project_source_root(), 'data'),
  'MESON_BUILD_ROOT': meson.project_build_root(),
  'MESON_BUILD_TEST_ROOT': meson.current_build_dir(),
  # Tests read the shipped data/common.conf, which carries the public registry
  # line. Without this, every test-launched server would announce to the live
  # registry (and fail its verify, since CI/test hosts aren't reachable).
  'DC_DISABLE_PUBLISH': '1',
  # Likewise, a test-launched server must not answer LAN discovery probes on the
  # tester's real network. (test_lan_discovery drives the discovery functions
  # directly, not via a spawned server reading this env, so it is unaffected.)
  'DC_DISABLE_LAN_DISCOVERY': '1',
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

# Standalone (own main, no 00_test.c): LAN discovery round trip. Uses loopback,
# so it is deterministic and does not need a port from base_port.
lan_discovery_exe = executable(
  'test_lan_discovery',
  'lan_discovery_test.c',
  dependencies: game_dep,
  override_options: ['b_lto=false'],
)
test('test_lan_discovery', lan_discovery_exe, env: environment(base_env))

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
  '5_card_triple_draw',
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
  env: environment(
    base_env + {
      'MESON_SOURCE_ROOT': meson.project_source_root(),
    },
  ),
  timeout: 180,
  depends: [pokeval_fuzz_exe],
)

# Fuzz harnesses for the three internet-facing wire parsers (#77): the game
# protocol framing + protobuf (net.c), the registry announce/list parse
# (net/registry.c), and LAN discovery (net/lan_discovery.c). Each feeds a seeded
# PRNG's random + mutated bytes at the parser and asserts no crash / OOB / UB.
# The whole point is to RUN them under the ASan/UBSan build, where any violation
# aborts. Standalone (own main, no 00_test.c). The net/lan harnesses bind
# ephemeral UDP/TCP loopback sockets (port 0), so they need no base_port slot.
#
# Count is deliberately modest by default (override with DC_FUZZ_N / seed with
# DC_FUZZ_SEED) so the suite stays fast under QEMU-emulated and BSD-VM CI, which
# run ~50x slower than native; a deeper offline run is `DC_FUZZ_N=2000000
# meson test ...`.
fuzz_parsers = [
  'net_fuzz',
  'registry_fuzz',
  'lan_fuzz',
]

foreach name : fuzz_parsers
  exe = executable(
    'test_' + name,
    name + '.c',
    dependencies: game_dep,
    override_options: ['b_lto=false'],
  )
  fuzz_env = base_env
  # net_fuzz drives a real loopback socket, so on slow CI runners (Windows, the
  # QEMU BSD VMs) the full iteration count can blow the test timeout. Cap its
  # wall-clock time instead: fast hosts still finish the whole count well under
  # the cap; slow ones stop early having fuzzed as many frames as fit. The
  # in-memory fuzzers (registry/lan) are fast everywhere and need no cap.
  if name == 'net_fuzz'
    fuzz_env += {'DC_FUZZ_MS': '60000'}
  endif
  test(
    'test_' + name,
    exe,
    env: environment(fuzz_env),
    timeout: 180,
  )
endforeach

add_test_setup(
  'Three_pass',
  #exe_wrapper: [join_paths(meson.current_source_dir(), '5_card_showdown.sh')],
  env: environment(
    base_env + {
      'DC_NPASSES': '3',
      # Suppress third-party libdbus/systemd lock-order inversions under TSan
      # (no-op for non-TSan binaries). See #282 and tsan_suppressions.txt.
      'TSAN_OPTIONS': 'suppressions=' + meson.current_source_dir() / 'tsan_suppressions.txt',
    },
  ),
)
