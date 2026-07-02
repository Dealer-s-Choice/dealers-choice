/*
 * test_error_threadlocal.c — tcpme_get_error() is documented as thread-local
 * (TCPME_TLS = _Thread_local / __declspec(thread); no relation to Transport
 * Layer Security).
 *
 * Two threads each trigger a *different* error, capture their error string,
 * sleep past the other thread's trigger, then re-read.  If the error store
 * were a shared global, the last writer would clobber the other thread's
 * message and the re-read comparison would fail.  main() additionally checks
 * the two captured messages actually differ, so a clobber can't hide behind
 * identical strings.
 */

#include "tcpme_test_helpers.h"
#include <stdio.h>
#include <string.h>

#include "tcpme.h"

#define ERRLEN 256

typedef struct {
  char captured[ERRLEN];
  int error;
} ThreadErrArg_t;

/* Trigger: tcpme_add_socket on a NULL set. */
static TC_THREAD_FN thread_a(void *varg) {
  ThreadErrArg_t *a = varg;

  assert(tcpme_add_socket(NULL, TCPME_INVALID_SOCKET) == -1);
  snprintf(a->captured, ERRLEN, "%s", tcpme_get_error());
  assert(strlen(a->captured) > 0);

  /* Let the other thread set its own (different) error... */
  tc_sleep_ms(200);

  /* ...then verify ours survived untouched. */
  if (strcmp(a->captured, tcpme_get_error()) != 0)
    a->error = 1;
  return TC_THREAD_RET;
}

/* Trigger: UDP sendto with an unparseable IPv4 literal. */
static TC_THREAD_FN thread_b(void *varg) {
  ThreadErrArg_t *a = varg;

  tcpme_socket_t s = tcpme_udp_open(0, false);
  assert(tcpme_socket_valid(s));
  assert(tcpme_udp_sendto(s, "not.an.ip", 1234, "x", 1) == -1);
  snprintf(a->captured, ERRLEN, "%s", tcpme_get_error());
  assert(strlen(a->captured) > 0);

  tc_sleep_ms(200);

  if (strcmp(a->captured, tcpme_get_error()) != 0)
    a->error = 1;
  tcpme_close(s);
  return TC_THREAD_RET;
}

int main(void) {
  assert(tcpme_init() == 0);

  ThreadErrArg_t a = {.error = 0};
  ThreadErrArg_t b = {.error = 0};
  tc_thread_t ta, tb;
  assert(tc_thread_create(&ta, thread_a, &a) == 0);
  assert(tc_thread_create(&tb, thread_b, &b) == 0);
  tc_thread_join(ta);
  tc_thread_join(tb);

  assert(a.error == 0);
  assert(b.error == 0);
  /* The two triggers produce different messages; if they ever converged the
   * clobber check above would be vacuous, so pin the difference here. */
  assert(strcmp(a.captured, b.captured) != 0);

  printf("thread A error: \"%s\"\n", a.captured);
  printf("thread B error: \"%s\"\n", b.captured);

  tcpme_quit();
  return 0;
}
