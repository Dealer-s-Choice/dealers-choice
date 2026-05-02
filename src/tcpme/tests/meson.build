foreach t : [
  'test_basic',
  'test_loopback',
  'test_socket_set',
  'test_error_paths',
  'test_hostname',
]
  exe = executable(
    'tcpme_' + t,
    t + '.c',
    dependencies: tcpme_dep,
  )
  test('tcpme_' + t, exe, suite: 'tcpme')
endforeach

threads_dep = dependency('threads')
exe = executable(
  'tcpme_test_protocol',
  'test_protocol.c',
  dependencies: [tcpme_dep, threads_dep],
)
test('tcpme_test_protocol', exe, suite: 'tcpme')
