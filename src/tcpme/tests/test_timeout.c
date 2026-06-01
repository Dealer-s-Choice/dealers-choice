/*
 * test_timeout.c — verifies tcpme_set_timeout bounds blocking recv so a
 * stalled peer can't hang the reader.
 *
 * If the timeout were not applied, the recv calls below would block forever
 * and the test harness would kill this run as a timeout — which is itself the
 * failure signal.  Reaching the final assert is the proof it works.
 */

#include "tcpme_test_helpers.h"
#include <stdio.h>

#include "tcpme.h"

int main(void) {
  assert(tcpme_init() == 0);

  tcpme_socket_t server = tcpme_listen("127.0.0.1", 0);
  assert(tcpme_socket_valid(server));

  char addr[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(server, addr, sizeof(addr)));
  uint16_t port = extract_port(addr);

  tcpme_socket_t client = tcpme_connect("127.0.0.1", port);
  assert(tcpme_socket_valid(client));

  tcpme_socket_t peer = tc_accept_retry(server);
  assert(tcpme_socket_valid(peer));

  /* Bound blocking I/O on the receiving side to a short timeout. */
  assert(tcpme_set_timeout(client, 200) == 0);

  char buf[64];

  /* Case 1 — timeout cap: peer sends nothing, so recv must time out
   * (return < 0) instead of blocking forever. */
  int r = tcpme_recv(client, buf, sizeof(buf));
  assert(r < 0);

  /* Case 2 — partial frame then stall: peer sends a 2-byte "header" and
   * stops.  The first recv returns that data; the next recv (waiting for the
   * rest of the frame) must time out rather than hang. */
  assert(tcpme_send(peer, "ab", 2) == 2);
  r = tcpme_recv(client, buf, sizeof(buf));
  assert(r > 0);
  r = tcpme_recv(client, buf, sizeof(buf));
  assert(r < 0);

  printf("tcpme_set_timeout bounds blocking recv (cap + partial-stall)\n");

  tcpme_close(client);
  tcpme_close(peer);
  tcpme_close(server);
  tcpme_quit();
  return 0;
}
