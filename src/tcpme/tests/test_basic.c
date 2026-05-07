/*
 * test_basic.c — tcpme init/quit, tcpme_socket_valid, error string,
 * and graceful failure paths.
 */

#include "tcpme_test_helpers.h"
#include <string.h>

#include "tcpme.h"

int main(void) {
  /* tcpme_socket_valid */
  assert(!tcpme_socket_valid(TCPME_INVALID_SOCKET));

  assert(tcpme_init() == 0);

  /* Connect to an address that should refuse/timeout quickly: nothing is
   * listening on loopback port 1 (reserved, always refused). */
  tcpme_socket_t s = tcpme_connect("127.0.0.1", 1);
  assert(!tcpme_socket_valid(s));
  assert(strlen(tcpme_get_error()) > 0);

  /* tcpme_alloc_set with invalid capacity must return NULL. */
  assert(tcpme_alloc_set(0) == NULL);
  assert(tcpme_alloc_set(-1) == NULL);

  /* tcpme_accept on an invalid socket must not crash and must return invalid. */
  assert(!tcpme_socket_valid(tcpme_accept(TCPME_INVALID_SOCKET)));

  /* tcpme_del_socket on NULL set must not crash and must set error. */
  tcpme_del_socket(NULL, TCPME_INVALID_SOCKET);
  assert(strlen(tcpme_get_error()) > 0);

  /* tcpme_check_sockets on NULL must return 0 without crashing. */
  assert(tcpme_check_sockets(NULL, 0) == 0);

  /* tcpme_socket_ready on NULL must return false. */
  assert(!tcpme_socket_ready(NULL, TCPME_INVALID_SOCKET));

  /* tcpme_close on TCPME_INVALID_SOCKET must not crash. */
  tcpme_close(TCPME_INVALID_SOCKET);

  tcpme_quit();
  return 0;
}
