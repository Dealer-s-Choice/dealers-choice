/* Pins down the sliding-window behavior of the per-IP connection rate
 * limiter (rate_limit_check in src/server.c).  Time is injected via
 * dc_rate_limit_check_at() so the test can verify window expiry without
 * sleeping. */

#include "00_test.h"

#define WINDOW_MS 60000u

static void test_burst_then_reject(void) {
  dc_rate_limit_reset();

  /* Five rapid attempts at t=0 with max=5: all admitted. */
  for (uint32_t i = 0; i < 5; i++)
    assert(dc_rate_limit_check_at("1.2.3.4", 5, 0));

  /* Sixth attempt within the window: rejected. */
  assert(!dc_rate_limit_check_at("1.2.3.4", 5, 1));

  /* Repeated attempts inside the window stay rejected — the rejected
   * attempt itself doesn't extend the window (no new entry recorded). */
  for (uint32_t t = 100; t < WINDOW_MS; t += 1000)
    assert(!dc_rate_limit_check_at("1.2.3.4", 5, t));
}

static void test_per_ip_counters_are_independent(void) {
  dc_rate_limit_reset();

  /* IP A hits its limit. */
  for (uint32_t i = 0; i < 3; i++)
    assert(dc_rate_limit_check_at("10.0.0.1", 3, i));
  assert(!dc_rate_limit_check_at("10.0.0.1", 3, 4));

  /* IP B is unaffected. */
  for (uint32_t i = 0; i < 3; i++)
    assert(dc_rate_limit_check_at("10.0.0.2", 3, 4 + i));
  assert(!dc_rate_limit_check_at("10.0.0.2", 3, 100));

  /* IP A still capped. */
  assert(!dc_rate_limit_check_at("10.0.0.1", 3, 200));
}

static void test_sliding_window_recovery(void) {
  dc_rate_limit_reset();

  /* Three attempts at t=0,1,2 with max=3 → all allowed. */
  for (uint32_t i = 0; i < 3; i++)
    assert(dc_rate_limit_check_at("172.16.0.1", 3, i));

  /* At t=3 we're at the cap → rejected. */
  assert(!dc_rate_limit_check_at("172.16.0.1", 3, 3));

  /* Still rejected just before the window slides off the oldest entry
   * (now - ticks(0) = 59999 < WINDOW_MS). */
  assert(!dc_rate_limit_check_at("172.16.0.1", 3, WINDOW_MS - 1));

  /* At t=WINDOW_MS, the t=0 entry is exactly at the boundary
   * (now - ticks == WINDOW_MS, not strictly <) and gets pruned.  Count
   * drops to 2 → allowed.  This is the user-visible "I got rejected,
   * then I could connect again" moment. */
  assert(dc_rate_limit_check_at("172.16.0.1", 3, WINDOW_MS));

  /* And — perhaps counterintuitively — the t=1 entry also slides off at
   * t=WINDOW_MS+1, so a second attempt right after also gets in.  This
   * is how the sliding window behaves when the original entries were
   * close together: they expire close together, giving another short
   * burst before the cap is hit again.  This is the exact behavior the
   * user described as "until the limit was hit again". */
  assert(dc_rate_limit_check_at("172.16.0.1", 3, WINDOW_MS + 1));
  assert(dc_rate_limit_check_at("172.16.0.1", 3, WINDOW_MS + 2));

  /* Now the three new entries are at t = WINDOW_MS, WINDOW_MS+1,
   * WINDOW_MS+2 — none yet expired — so we're back at the cap. */
  assert(!dc_rate_limit_check_at("172.16.0.1", 3, WINDOW_MS + 3));
}

static void test_higher_max_admits_more(void) {
  dc_rate_limit_reset();

  /* 10-per-minute permits a 10-burst; 11th is rejected. */
  for (uint32_t i = 0; i < 10; i++)
    assert(dc_rate_limit_check_at("192.168.1.1", 10, i));
  assert(!dc_rate_limit_check_at("192.168.1.1", 10, 11));
}

_MAIN_HEAD_
(void)argc;
(void)argv;
test_burst_then_reject();
test_per_ip_counters_are_independent();
test_sliding_window_recovery();
test_higher_max_admits_more();
fprintf(stderr, "rate-limit tests: OK\n");
_MAIN_TAIL_
