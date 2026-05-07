/*
 * test_error_paths.c — address query functions on an invalid socket,
 * tcpme_add_socket(NULL, ...), and tcpme_check_sockets on an empty set.
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

  tcpme_quit();
  return 0;
}
