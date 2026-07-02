foreach t : [
  'basic',
  'bytes',
  'connect_timeout',
  'loopback',
  'ipv6',
  'socket_set',
  'error_paths',
  'hostname',
  'peer_close',
  'timeout',
  'udp',
  'udp6',
]
  exe = executable(
    'tcpme_test_' + t,
    'test_' + t + '.c',
    dependencies: tcpme_dep,
  )
  test('tcpme_test_' + t, exe, suite: 'tcpme')
endforeach

# Tests that spawn threads (pthreads / Win32 via tcpme_test_helpers.h).
threads_dep = dependency('threads')
foreach t : ['protocol', 'error_threadlocal']
  exe = executable(
    'tcpme_test_' + t,
    'test_' + t + '.c',
    dependencies: [tcpme_dep, threads_dep],
  )
  test('tcpme_test_' + t, exe, suite: 'tcpme')
endforeach
