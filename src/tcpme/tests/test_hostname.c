/*
 * test_hostname.c — exercises two code paths the other tests skip:
 *   1. tcpme_connect with a hostname ("localhost") rather than an IP literal,
 *      which forces getaddrinfo to do a real name lookup.
 *   2. tcpme_listen with host=NULL (INADDR_ANY / in6addr_any).
 */

#include "tcpme_test_helpers.h"
#include <stdio.h>
#include <string.h>

#include "tcpme.h"

int main(void) {
  assert(tcpme_init() == 0);

  /* --- 1. Hostname resolution: connect via "localhost" --- */

  tcpme_socket_t server = tcpme_listen("127.0.0.1", 0);
  assert(tcpme_socket_valid(server));

  char local_addr[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(server, local_addr, sizeof(local_addr)));
  uint16_t port = extract_port(local_addr);
  printf("server listening at %s\n", local_addr);

  /* Connect using a hostname; getaddrinfo must resolve it. */
  tcpme_socket_t client = tcpme_connect("localhost", port);
  assert(tcpme_socket_valid(client));

  tcpme_socket_t peer = tc_accept_retry(server);
  assert(tcpme_socket_valid(peer));

  const char msg[] = "hostname test";
  assert(tcpme_send(client, msg, (int)sizeof(msg)) == (int)sizeof(msg));
  char buf[64] = {0};
  assert(tcpme_recv(peer, buf, (int)(sizeof(buf) - 1)) == (int)sizeof(msg));
  assert(strcmp(buf, msg) == 0);
  printf("received: \"%s\"\n", buf);

  tcpme_close(peer);
  tcpme_close(client);
  tcpme_close(server);

  /* --- 2. NULL host: listen on all interfaces --- */

  tcpme_socket_t all_server = tcpme_listen(NULL, 0);
  assert(tcpme_socket_valid(all_server));

  char all_addr[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(all_server, all_addr, sizeof(all_addr)));
  uint16_t all_port = extract_port(all_addr);
  printf("all-interfaces server at %s\n", all_addr);

  /* A dual-stack IPv6 socket accepts IPv4-mapped connections; an IPv4-only
   * socket accepts IPv4 connections. Try both loopback addresses so the test
   * passes on both configurations. */
  tcpme_socket_t c2 = tcpme_connect("127.0.0.1", all_port);
  if (!tcpme_socket_valid(c2))
    c2 = tcpme_connect("::1", all_port);
  assert(tcpme_socket_valid(c2));

  tcpme_socket_t p2 = tc_accept_retry(all_server);
  assert(tcpme_socket_valid(p2));

  tcpme_close(p2);
  tcpme_close(c2);
  tcpme_close(all_server);

  tcpme_quit();
  return 0;
}
