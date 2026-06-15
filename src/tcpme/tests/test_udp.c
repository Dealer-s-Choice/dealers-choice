/*
 * test_udp.c — IPv4 UDP datagram primitives: open (ephemeral + broadcast
 * modes), the discovery-style round trip (client unicasts a query to the
 * server, the server learns the sender from recvfrom and unicasts a reply
 * back), and the invalid-address error path.
 *
 * Broadcast *delivery* (255.255.255.255) is environment-dependent, so it is
 * exercised at the feature level rather than asserted here; the unicast round
 * trip covers every UDP function on reliable loopback.
 */

#include "tcpme_test_helpers.h"

#include "tcpme.h"

/* Wait up to ~1s for sock to become readable; abort the test if it never
 * does (so a broken recvfrom path fails fast instead of hanging). */
static void wait_readable(tcpme_socket_t sock) {
  tcpme_set_t *set = tcpme_alloc_set(1);
  assert(set != NULL);
  assert(tcpme_add_socket(set, sock) == 0);
  int n = tcpme_check_sockets(set, 1000);
  assert(n == 1);
  assert(tcpme_socket_ready(set, sock));
  tcpme_free_set(set);
}

int main(void) {
  assert(tcpme_init() == 0);

  /* Server: bound to an ephemeral port we read back. */
  tcpme_socket_t server = tcpme_udp_open(0, false);
  assert(tcpme_socket_valid(server));
  char saddr[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(server, saddr, sizeof(saddr)));
  uint16_t server_port = extract_port(saddr);

  /* Client: broadcast-enabled, ephemeral. */
  tcpme_socket_t client = tcpme_udp_open(0, true);
  assert(tcpme_socket_valid(client));
  char caddr[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(client, caddr, sizeof(caddr)));
  uint16_t client_port = extract_port(caddr);

  /* Client -> server query (unicast to loopback). */
  const char query[] = "DCLAN?";
  assert(tcpme_udp_sendto(client, "127.0.0.1", server_port, query, (int)sizeof(query)) ==
         (int)sizeof(query));

  /* Server receives it and learns the sender's address. */
  wait_readable(server);
  char buf[64];
  char from_ip[TCPME_ADDRSTRLEN];
  uint16_t from_port = 0;
  int n = tcpme_udp_recvfrom(server, buf, sizeof(buf), from_ip, sizeof(from_ip), &from_port);
  assert(n == (int)sizeof(query));
  assert(memcmp(buf, query, sizeof(query)) == 0);
  assert(strcmp(from_ip, "127.0.0.1") == 0);
  assert(from_port == client_port);

  /* Server -> client unicast reply, addressed from what recvfrom reported. */
  const char reply[] = "DCLAN!";
  assert(tcpme_udp_sendto(server, from_ip, from_port, reply, (int)sizeof(reply)) ==
         (int)sizeof(reply));

  /* Client receives the reply. */
  wait_readable(client);
  char rbuf[64];
  char rip[TCPME_ADDRSTRLEN];
  uint16_t rport = 0;
  int rn = tcpme_udp_recvfrom(client, rbuf, sizeof(rbuf), rip, sizeof(rip), &rport);
  assert(rn == (int)sizeof(reply));
  assert(memcmp(rbuf, reply, sizeof(reply)) == 0);
  assert(strcmp(rip, "127.0.0.1") == 0);
  assert(rport == server_port);

  /* Error path: invalid IPv4 literal must fail without sending. */
  assert(tcpme_udp_sendto(client, "not.an.ip", 1234, query, (int)sizeof(query)) == -1);
  assert(strlen(tcpme_get_error()) > 0);

  tcpme_close(client);
  tcpme_close(server);
  tcpme_quit();
  return 0;
}
