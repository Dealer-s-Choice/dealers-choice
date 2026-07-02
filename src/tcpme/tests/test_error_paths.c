/*
 * test_error_paths.c — address query functions on an invalid socket,
 * tcpme_add_socket(NULL, ...), tcpme_check_sockets on an empty set,
 * degenerate send/recv lengths, and double-listen on a bound port.
 */

#include "tcpme_test_helpers.h"
#include <string.h>

#include "tcpme.h"

int main(void) {
  assert(tcpme_init() == 0);

  char buf[TCPME_ADDRPORTSTRLEN];

  /* Address queries on TCPME_INVALID_SOCKET must return false and set error. */
  assert(!tcpme_get_peer_addr(TCPME_INVALID_SOCKET, buf, sizeof(buf)));
  assert(strlen(tcpme_get_error()) > 0);

  assert(!tcpme_get_local_addr(TCPME_INVALID_SOCKET, buf, sizeof(buf)));
  assert(strlen(tcpme_get_error()) > 0);

  assert(!tcpme_get_peer_ip(TCPME_INVALID_SOCKET, buf, sizeof(buf)));
  assert(strlen(tcpme_get_error()) > 0);

  /* tcpme_add_socket with a NULL set must return -1 and set error. */
  assert(tcpme_add_socket(NULL, TCPME_INVALID_SOCKET) == -1);
  assert(strlen(tcpme_get_error()) > 0);

  /* tcpme_check_sockets on an allocated but empty set must return 0. */
  tcpme_set_t *set = tcpme_alloc_set(4);
  assert(set != NULL);
  assert(tcpme_check_sockets(set, 0) == 0);
  tcpme_free_set(set);

  /* --- Degenerate send/recv lengths on a real connected pair --- */

  tcpme_socket_t server = tcpme_listen("127.0.0.1", 0);
  assert(tcpme_socket_valid(server));
  char addr[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(server, addr, sizeof(addr)));
  uint16_t port = extract_port(addr);

  tcpme_socket_t client = tcpme_connect("127.0.0.1", port);
  assert(tcpme_socket_valid(client));
  tcpme_socket_t peer = tc_accept_retry(server);
  assert(tcpme_socket_valid(peer));

  /* Degenerate lengths are validated by tcpme itself (see the guards in
   * tcpme_send/tcpme_recv): without them, a negative len becomes a huge
   * size_t on POSIX and the kernel *services* it (clamped to ~2 GB) — recv
   * blocks waiting for data instead of failing.  These asserts pin the
   * boundary contract: len == 0 is a no-op returning 0, len < 0 fails fast
   * with an error string, identically on POSIX and Winsock. */
  char dbuf[8] = {0};
  assert(tcpme_send(client, dbuf, 0) == 0);
  assert(tcpme_recv(peer, dbuf, 0) == 0);

  assert(tcpme_send(client, dbuf, -1) < 0);
  assert(strlen(tcpme_get_error()) > 0);
  assert(tcpme_recv(peer, dbuf, -1) < 0);
  assert(strlen(tcpme_get_error()) > 0);

  /* --- Double-listen on an already-bound port must fail --- */

#ifndef _WIN32
  /* Not asserted on Windows: tcpme_listen sets SO_REUSEADDR, and Winsock's
   * SO_REUSEADDR (unlike POSIX) allows a second active bind to the same
   * port, so the second listen can legitimately succeed there. */
  tcpme_socket_t dup = tcpme_listen("127.0.0.1", port);
  assert(!tcpme_socket_valid(dup));
  assert(strlen(tcpme_get_error()) > 0);
#endif

  tcpme_close(peer);
  tcpme_close(client);
  tcpme_close(server);

  tcpme_quit();
  return 0;
}
