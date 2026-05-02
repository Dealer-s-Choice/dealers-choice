/*
 * test_loopback.c — single-process listen/connect/accept/send/recv test on
 * 127.0.0.1. Uses port 0 so the OS picks a free port, then queries it back
 * via tcpme_get_local_addr before connecting.
 *
 * Also exercises tcpme_get_peer_ip and tcpme_get_peer_addr.
 */

#include "tcpme_test_helpers.h"
#include <stdio.h>
#include <string.h>

#include "tcpme.h"

int main(void) {
  assert(tcpme_init() == 0);

  /* Listen on loopback, port 0 → OS assigns a free port. */
  tcpme_socket_t server = tcpme_listen("127.0.0.1", 0);
  assert(tcpme_socket_valid(server));

  /* Find the actual bound port. */
  char local_addr[TCPME_ADDRSTRLEN + 16];
  assert(tcpme_get_local_addr(server, local_addr, sizeof(local_addr)));
  printf("server listening at %s\n", local_addr);
  uint16_t port = extract_port(local_addr);

  tcpme_socket_t client = tcpme_connect("127.0.0.1", port);
  assert(tcpme_socket_valid(client));

  tcpme_socket_t peer = tc_accept_retry(server);
  assert(tcpme_socket_valid(peer));

  /* Verify peer IP seen from the server side. */
  char ip[TCPME_ADDRSTRLEN];
  assert(tcpme_get_peer_ip(peer, ip, sizeof(ip)));
  printf("server sees peer IP: %s\n", ip);
  assert(strcmp(ip, "127.0.0.1") == 0 || strcmp(ip, "::1") == 0);

  /* Verify peer addr (IP:port) seen from the server side. */
  char peer_addr[TCPME_ADDRSTRLEN + 16];
  assert(tcpme_get_peer_addr(peer, peer_addr, sizeof(peer_addr)));
  printf("server sees peer addr: %s\n", peer_addr);
  /* Port portion must be non-zero — it's the ephemeral port the client used. */
  assert(extract_port(peer_addr) > 0);

  /* Verify peer addr seen from the client side points back to the server. */
  char server_addr[TCPME_ADDRSTRLEN + 16];
  assert(tcpme_get_peer_addr(client, server_addr, sizeof(server_addr)));
  printf("client sees server addr: %s\n", server_addr);
  assert(extract_port(server_addr) == port);

  /* Send from client → server. */
  const char msg[] = "hello tcpme";
  int n = tcpme_send(client, msg, (int)sizeof(msg));
  assert(n == (int)sizeof(msg));

  /* Recv on server peer socket. */
  char buf[64] = {0};
  n = tcpme_recv(peer, buf, (int)(sizeof(buf) - 1));
  assert(n == (int)sizeof(msg));
  assert(strcmp(buf, msg) == 0);
  printf("server received: \"%s\"\n", buf);

  /* Send from server → client. */
  const char reply[] = "world";
  n = tcpme_send(peer, reply, (int)sizeof(reply));
  assert(n == (int)sizeof(reply));

  memset(buf, 0, sizeof(buf));
  n = tcpme_recv(client, buf, (int)(sizeof(buf) - 1));
  assert(n == (int)sizeof(reply));
  assert(strcmp(buf, reply) == 0);
  printf("client received: \"%s\"\n", buf);

  /* accept() on a socket with no pending connection must return invalid. */
  assert(!tcpme_socket_valid(tcpme_accept(server)));

  tcpme_close(peer);
  tcpme_close(client);
  tcpme_close(server);

  /* Verify recv on closed socket returns ≤ 0. */
  char tmp[4];
  assert(tcpme_recv(peer, tmp, sizeof(tmp)) <= 0);

  tcpme_quit();
  return 0;
}
