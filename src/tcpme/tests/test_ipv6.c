/*
 * test_ipv6.c — IPv6 loopback test on ::1.
 *
 * Probes for IPv6 availability by attempting to listen on ::1. If the OS has
 * no IPv6 loopback the test exits 0 (skip) rather than failing, so it is safe
 * to run everywhere.
 */

#include "tcpme_test_helpers.h"
#include <stdio.h>
#include <string.h>

#include "tcpme.h"

int main(void) {
  assert(tcpme_init() == 0);

  tcpme_socket_t server = tcpme_listen("::1", 0);
  if (!tcpme_socket_valid(server)) {
    printf("IPv6 not available, skipping\n");
    tcpme_quit();
    return 77;
  }

  char local_addr[TCPME_ADDRSTRLEN + 16];
  assert(tcpme_get_local_addr(server, local_addr, sizeof(local_addr)));
  printf("server listening at %s\n", local_addr);
  uint16_t port = extract_port(local_addr);

  tcpme_socket_t client = tcpme_connect("::1", port);
  assert(tcpme_socket_valid(client));

  tcpme_socket_t peer = tc_accept_retry(server);
  assert(tcpme_socket_valid(peer));

  char ip[TCPME_ADDRSTRLEN];
  assert(tcpme_get_peer_ip(peer, ip, sizeof(ip)));
  printf("server sees peer IP: %s\n", ip);
  assert(strcmp(ip, "::1") == 0);

  char peer_addr[TCPME_ADDRSTRLEN + 16];
  assert(tcpme_get_peer_addr(peer, peer_addr, sizeof(peer_addr)));
  printf("server sees peer addr: %s\n", peer_addr);
  assert(extract_port(peer_addr) > 0);

  char server_addr[TCPME_ADDRSTRLEN + 16];
  assert(tcpme_get_peer_addr(client, server_addr, sizeof(server_addr)));
  printf("client sees server addr: %s\n", server_addr);
  assert(extract_port(server_addr) == port);

  const char msg[] = "hello ipv6";
  int n = tcpme_send(client, msg, (int)sizeof(msg));
  assert(n == (int)sizeof(msg));

  char buf[64] = {0};
  n = tcpme_recv(peer, buf, (int)(sizeof(buf) - 1));
  assert(n == (int)sizeof(msg));
  assert(strcmp(buf, msg) == 0);
  printf("server received: \"%s\"\n", buf);

  tcpme_close(peer);
  tcpme_close(client);
  tcpme_close(server);

  tcpme_quit();
  return 0;
}
