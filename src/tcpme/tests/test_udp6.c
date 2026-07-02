/*
 * test_udp6.c — IPv6 UDP primitives: open6, sendto6/recvfrom6 over ::1
 * (the discovery-style query/reply round trip, mirroring test_udp.c), the
 * invalid-address error path, and a best-effort multicast leg.
 *
 * These are the functions behind IPv6 LAN discovery (lan_discovery.c), so
 * the unicast round trip is asserted hard.  Multicast *delivery* depends on
 * the environment (interfaces, kernel config, VM networking), so that leg
 * exercises join_all/send_all under the sanitizers and only asserts delivery
 * when the environment cooperates — same philosophy as the IPv4 broadcast
 * note in test_udp.c.
 *
 * If the host has no IPv6 at all, exits 77 (meson SKIP), like test_ipv6.c.
 */

#include "tcpme_test_helpers.h"
#include <stdio.h>

#include "tcpme.h"

/* Any-source test group, link-local scope. */
#define TEST_GROUP "ff02::114"

static int wait_readable_ms(tcpme_socket_t sock, uint32_t ms) {
  tcpme_set_t *set = tcpme_alloc_set(1);
  assert(set != NULL);
  assert(tcpme_add_socket(set, sock) == 0);
  int n = tcpme_check_sockets(set, ms);
  int ready = (n == 1) && tcpme_socket_ready(set, sock);
  tcpme_free_set(set);
  return ready;
}

int main(void) {
  assert(tcpme_init() == 0);

  /* Probe IPv6 availability: no v6 stack → the socket/bind fails → skip. */
  tcpme_socket_t server = tcpme_udp_open6(0);
  if (!tcpme_socket_valid(server)) {
    printf("IPv6 UDP not available, skipping\n");
    tcpme_quit();
    return 77;
  }

  char saddr[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(server, saddr, sizeof(saddr)));
  uint16_t server_port = extract_port(saddr);

  tcpme_socket_t client = tcpme_udp_open6(0);
  assert(tcpme_socket_valid(client));
  char caddr[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(client, caddr, sizeof(caddr)));
  uint16_t client_port = extract_port(caddr);

  /* --- Unicast round trip over ::1 (hard asserts) --- */

  const char query[] = "DCLAN6?";
  assert(tcpme_udp_sendto6(client, "::1", 0, server_port, query, (int)sizeof(query)) ==
         (int)sizeof(query));

  assert(wait_readable_ms(server, 1000));
  char buf[64];
  char from_ip[TCPME_ADDRSTRLEN];
  unsigned from_scope = 1234; /* poison: recvfrom6 must overwrite it */
  uint16_t from_port = 0;
  int n = tcpme_udp_recvfrom6(server, buf, sizeof(buf), from_ip, sizeof(from_ip), &from_scope,
                              &from_port);
  assert(n == (int)sizeof(query));
  assert(memcmp(buf, query, sizeof(query)) == 0);
  assert(strcmp(from_ip, "::1") == 0);
  assert(from_scope == 0); /* ::1 is not link-local; scope must be 0 */
  assert(from_port == client_port);

  /* Reply addressed from what recvfrom6 reported (ip + scope + port). */
  const char reply[] = "DCLAN6!";
  assert(tcpme_udp_sendto6(server, from_ip, from_scope, from_port, reply, (int)sizeof(reply)) ==
         (int)sizeof(reply));

  assert(wait_readable_ms(client, 1000));
  char rbuf[64];
  char rip[TCPME_ADDRSTRLEN];
  unsigned rscope = 0;
  uint16_t rport = 0;
  int rn = tcpme_udp_recvfrom6(client, rbuf, sizeof(rbuf), rip, sizeof(rip), &rscope, &rport);
  assert(rn == (int)sizeof(reply));
  assert(memcmp(rbuf, reply, sizeof(reply)) == 0);
  assert(strcmp(rip, "::1") == 0);
  assert(rport == server_port);

  /* --- Error paths: unparseable addresses (hard asserts) --- */

  assert(tcpme_udp_sendto6(client, "not:an:ip::zz", 0, 1234, query, (int)sizeof(query)) == -1);
  assert(strlen(tcpme_get_error()) > 0);

  assert(tcpme_mcast6_join_all(client, "not-a-group") == 0);
  assert(strlen(tcpme_get_error()) > 0);
  assert(tcpme_udp_mcast6_send_all(client, "not-a-group", 1234, query, (int)sizeof(query)) == 0);
  assert(strlen(tcpme_get_error()) > 0);

  /* --- Multicast leg (best-effort: environment-dependent delivery) --- */

  int joined = tcpme_mcast6_join_all(server, TEST_GROUP);
  printf("mcast6_join_all: %d join(s)\n", joined);
  if (joined > 0) {
    const char mq[] = "DCLAN6-MC?";
    int sent = tcpme_udp_mcast6_send_all(client, TEST_GROUP, server_port, mq, (int)sizeof(mq));
    printf("mcast6_send_all: sent on %d interface(s)\n", sent);
    if (sent > 0 && wait_readable_ms(server, 1000)) {
      n = tcpme_udp_recvfrom6(server, buf, sizeof(buf), from_ip, sizeof(from_ip), &from_scope,
                              &from_port);
      assert(n == (int)sizeof(mq));
      assert(memcmp(buf, mq, sizeof(mq)) == 0);
      printf("multicast delivered from %s (scope %u)\n", from_ip, from_scope);
    } else {
      printf("multicast delivery unavailable here; join/send paths exercised only\n");
    }
  }

  tcpme_close(client);
  tcpme_close(server);
  tcpme_quit();
  return 0;
}
