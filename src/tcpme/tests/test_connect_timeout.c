/*
 * test_connect_timeout.c — tcpme_connect_timeout and _pref4.
 *
 * Three cases:
 *   1. Success over loopback: connects fast, and the returned socket is
 *      restored to blocking mode and usable (send/recv round trip).
 *   2. Refused: nothing listening → fails with an error string, and does so
 *      *immediately* (loopback RST), without consuming the timeout budget.
 *   3. Deadline: a blackholed target must give up by roughly the requested
 *      timeout.  Only the upper bound is asserted — depending on the CI
 *      network the target may fail fast instead (ICMP unreachable, no
 *      route), which is fine; what must never happen is blocking far past
 *      the deadline, since that is the function's entire contract.
 */

#include "tcpme_test_helpers.h"
#include <stdio.h>
#include <string.h>

#include "tcpme.h"

int main(void) {
  tc_test_init();
  assert(tcpme_init() == 0);

  /* --- 1. Success path (both variants) --- */

  tcpme_socket_t server = tcpme_listen("127.0.0.1", 0);
  assert(tcpme_socket_valid(server));
  char addr[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(server, addr, sizeof(addr)));
  uint16_t port = extract_port(addr);

  tcpme_socket_t c1 = tcpme_connect_timeout("127.0.0.1", port, 2000);
  assert(tcpme_socket_valid(c1));
  tcpme_socket_t p1 = tc_accept_retry(server);
  assert(tcpme_socket_valid(p1));

  /* The connect path flips the socket non-blocking for the timed connect and
   * must restore blocking mode — prove it with a normal exchange. */
  const char msg[] = "ct";
  assert(tcpme_send(c1, msg, (int)sizeof(msg)) == (int)sizeof(msg));
  char buf[8] = {0};
  assert(tcpme_recv(p1, buf, (int)(sizeof(buf) - 1)) == (int)sizeof(msg));
  assert(strcmp(buf, msg) == 0);
  tcpme_close(p1);
  tcpme_close(c1);

  tcpme_socket_t c2 = tcpme_connect_timeout_pref4("127.0.0.1", port, 2000);
  assert(tcpme_socket_valid(c2));
  tcpme_socket_t p2 = tc_accept_retry(server);
  assert(tcpme_socket_valid(p2));
  tcpme_close(p2);
  tcpme_close(c2);
  tcpme_close(server);

  /* --- 2. Refused: must fail fast, not eat the timeout --- */

  /* The listener above is closed, so its port is free again; use the
   * always-refused reserved port 1 like test_basic.c does. */
  int64_t t0 = tc_now_ms();
  tcpme_socket_t r = tcpme_connect_timeout("127.0.0.1", 1, 10000);
  int64_t elapsed = tc_now_ms() - t0;
  assert(!tcpme_socket_valid(r));
  assert(strlen(tcpme_get_error()) > 0);
  printf("refused connect returned in %lld ms (budget 10000): %s\n", (long long)elapsed,
         tcpme_get_error());
  /* Loopback refusal is an immediate RST; 5s of slack for slow VMs is still
   * far below the 10s budget, so a refusal that waits out the timeout fails.
   * (This is the assert that caught Winsock reporting a failed non-blocking
   * connect only in select's except set -- see try_connect_addrinfo.) */
  assert(elapsed < 5000);

  /* --- 3. Deadline against a blackholed target --- */

  /* 192.0.2.1 (RFC 5737 TEST-NET-1) is reserved for documentation and is
   * blackholed on most networks.  Some environments reject it fast instead
   * (no route / ICMP unreachable) — either way the call must be back well
   * before ~timeout + slack. */
  t0 = tc_now_ms();
  r = tcpme_connect_timeout("192.0.2.1", 9, 300);
  elapsed = tc_now_ms() - t0;
  assert(!tcpme_socket_valid(r));
  assert(strlen(tcpme_get_error()) > 0);
  assert(elapsed < 5000);
  printf("blackhole connect returned in %lld ms (timeout 300)\n", (long long)elapsed);

  tcpme_quit();
  return 0;
}
