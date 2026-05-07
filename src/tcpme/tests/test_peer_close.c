/*
 * test_peer_close.c — verifies that tcpme_send does not deliver SIGPIPE
 * when writing to a socket whose peer has closed.
 *
 * Without MSG_NOSIGNAL (Linux) or SO_NOSIGPIPE (macOS/BSD) the second send
 * would deliver SIGPIPE and kill the process before reaching the final assert.
 * Reaching it is the proof that suppression works.
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

  /* Close the server-side socket; client doesn't know yet. */
  tcpme_close(peer);

  /* Drain the FIN so the client TCP stack knows the peer is gone. */
  char buf[64];
  tcpme_recv(client, buf, sizeof(buf));

  /* Two sends: the first may be buffered by the kernel, the second triggers
   * RST / EPIPE. If SIGPIPE is not suppressed the process dies here. */
  tcpme_send(client, "x", 1);
  tcpme_send(client, "x", 1);

  printf("SIGPIPE suppressed: process survived writes to closed peer\n");

  tcpme_close(client);
  tcpme_close(server);
  tcpme_quit();
  return 0;
}
