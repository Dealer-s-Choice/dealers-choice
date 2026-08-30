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
#include <stdio.h>
#include <string.h>

#include "tcpme.h"

/* Wait up to ~1s for sock to become readable; abort the test if it never
 * does (so a broken recvfrom path fails fast instead of hanging). */
int main(void) {
  tc_test_init();
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
  assert(tc_wait_readable(server, 1000));
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
  assert(tc_wait_readable(client, 1000));
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

  /* Broadcast send path: delivery is environment-dependent (see header), but
   * the *send* itself must either succeed or fail cleanly with an error set —
   * some sandboxes have no broadcast route, so both outcomes are accepted. */
  int bn = tcpme_udp_broadcast(client, server_port, query, (int)sizeof(query));
  if (bn == (int)sizeof(query)) {
    printf("broadcast send succeeded\n");
  } else {
    assert(bn == -1);
    assert(strlen(tcpme_get_error()) > 0);
    printf("broadcast send unavailable here: %s\n", tcpme_get_error());
  }

  /* Degenerate lengths: the UDP entry points carry the same boundary guard as
   * tcpme_send/tcpme_recv (TCPME_REJECT_NEG_LEN), because a negative int becomes
   * a huge size_t on POSIX and the kernel services it rather than rejecting it.
   * The TCP side is asserted in test_error_paths.c; these are the UDP mirror.
   * tcpme_udp_mcast6_send_all is the odd one out -- it returns an interface
   * count, so its rejection value is 0 rather than -1; that one is asserted in
   * test_udp6.c. */
  assert(tcpme_udp_sendto(client, "127.0.0.1", server_port, query, -1) == -1);
  assert(strlen(tcpme_get_error()) > 0);

  char nbuf[8] = {0};
  char nip[TCPME_ADDRSTRLEN];
  uint16_t nport = 0;
  assert(tcpme_udp_recvfrom(client, nbuf, -1, nip, sizeof(nip), &nport) == -1);
  assert(strlen(tcpme_get_error()) > 0);

  assert(tcpme_udp_broadcast(client, server_port, query, -1) == -1);
  assert(strlen(tcpme_get_error()) > 0);

  tcpme_close(client);
  tcpme_close(server);
  tcpme_quit();
  return 0;
}
