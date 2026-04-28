foreach t : ['test_basic', 'test_loopback', 'test_socket_set']
  exe = executable(
    'tcpme_' + t,
    t + '.c',
    dependencies: tcpme_dep,
  )
  test('tcpme_' + t, exe, suite: 'tcpme')
endforeach
