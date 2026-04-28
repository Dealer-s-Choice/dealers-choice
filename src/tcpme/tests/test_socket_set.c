/*
 * test_socket_set.c — tcpme_set_t lifecycle: alloc, add, check_sockets,
 * socket_ready, del, free. Uses a loopback pair as concrete sockets.
 */

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcpme.h"

static uint16_t extract_port(const char *addr) {
  const char *colon = strrchr(addr, ':');
  assert(colon != NULL);
  int p = atoi(colon + 1);
  assert(p > 0 && p <= 65535);
  return (uint16_t)p;
}

int main(void) {
  assert(tcpme_init() == 0);

  tcpme_socket_t server = tcpme_listen("127.0.0.1", 0);
  assert(tcpme_socket_valid(server));

  char local_addr[TCPME_ADDRSTRLEN + 16];
  assert(tcpme_get_local_addr(server, local_addr, sizeof(local_addr)));
  uint16_t port = extract_port(local_addr);

  tcpme_socket_t client = tcpme_connect("127.0.0.1", port);
  assert(tcpme_socket_valid(client));

  tcpme_socket_t peer = tcpme_accept(server);
  assert(tcpme_socket_valid(peer));

  /* --- set operations --- */

  tcpme_set_t *set = tcpme_alloc_set(2);
  assert(set != NULL);

  assert(tcpme_add_socket(set, client) == 0);
  assert(tcpme_add_socket(set, peer) == 0);

  /* Set is full: adding another socket must fail. */
  assert(tcpme_add_socket(set, server) == -1);

  /* Nothing in the buffers yet: check with zero timeout → 0 ready. */
  int ready = tcpme_check_sockets(set, 0);
  assert(ready == 0);
  assert(!tcpme_socket_ready(set, client));
  assert(!tcpme_socket_ready(set, peer));

  /* Send from client so peer becomes readable. */
  const char msg[] = "set test";
  assert(tcpme_send(client, msg, (int)sizeof(msg)) == (int)sizeof(msg));

  /* Wait up to 500 ms for data to arrive. */
  ready = tcpme_check_sockets(set, 500);
  assert(ready == 1);
  assert(tcpme_socket_ready(set, peer));
  assert(!tcpme_socket_ready(set, client));

  /* Drain the data. */
  char buf[64] = {0};
  int n = tcpme_recv(peer, buf, (int)(sizeof(buf) - 1));
  assert(n == (int)sizeof(msg));
  assert(strcmp(buf, msg) == 0);
  printf("received: \"%s\"\n", buf);

  /* del_socket removes client; peer should still be present. */
  assert(tcpme_del_socket(set, client) == 0);

  /* Deleting a socket not in the set must fail. */
  assert(tcpme_del_socket(set, client) == -1);

  /* Send again so the remaining peer socket becomes readable. */
  assert(tcpme_send(client, msg, (int)sizeof(msg)) == (int)sizeof(msg));
  ready = tcpme_check_sockets(set, 500);
  assert(ready == 1);
  assert(tcpme_socket_ready(set, peer));

  tcpme_free_set(set);

  /* tcpme_free_set(NULL) must not crash. */
  tcpme_free_set(NULL);

  tcpme_close(peer);
  tcpme_close(client);
  tcpme_close(server);

  tcpme_quit();
  return 0;
}
