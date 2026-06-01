foreach t : [
  'basic',
  'loopback',
  'ipv6',
  'socket_set',
  'error_paths',
  'hostname',
  'peer_close',
  'timeout',
]
  exe = executable(
    'tcpme_test_' + t,
    'test_' + t + '.c',
    dependencies: tcpme_dep,
  )
  test('tcpme_test_' + t, exe, suite: 'tcpme')
endforeach

threads_dep = dependency('threads')
exe = executable(
  'tcpme_test_protocol',
  'test_protocol.c',
  dependencies: [tcpme_dep, threads_dep],
)
test('tcpme_test_protocol', exe, suite: 'tcpme')
