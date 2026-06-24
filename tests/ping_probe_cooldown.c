/* Pins the per-target cooldown of the connect-screen ping probe: a server is
 * re-probed at most once per cooldown no matter how often the UI re-pushes its
 * (flapping) target list. That cap is what stops a browsing client from
 * flooding a server with connections and tripping its per-IP rate limit.
 *
 * Time is injected via ping_probe_due_at() so the rule is verified instantly,
 * without spinning the worker thread or sleeping. The end-to-end "hundreds of
 * list pushes -> a handful of connections" proof is tests/ping_probe_flood.c. */

#include "00_test.h"

#include "ping_probe.h"

#define COOLDOWN_MS 10000u

static void test_never_probed_is_due(void) {
  /* last_probe_ticks == 0 means "never probed" -> always due, even at t=0. */
  assert(ping_probe_due_at(0, 0, COOLDOWN_MS));
  assert(ping_probe_due_at(0, 123456, COOLDOWN_MS));
}

static void test_within_cooldown_not_due(void) {
  /* Probed at t=1000; every ~frame push across the whole cooldown must NOT
   * re-probe. This is the exact flap that used to flood: hundreds of pushes,
   * zero extra probes. */
  for (uint32_t now = 1000; now < 1000 + COOLDOWN_MS; now += 16)
    assert(!ping_probe_due_at(1000, now, COOLDOWN_MS));
}

static void test_just_probed_not_due(void) {
  /* Re-pushed the same frame (elapsed 0) -> not due. */
  assert(!ping_probe_due_at(5000, 5000, COOLDOWN_MS));
}

static void test_cooldown_boundary_is_due(void) {
  /* Exactly at the interval (elapsed == interval) it's due again, and after. */
  assert(ping_probe_due_at(1000, 1000 + COOLDOWN_MS, COOLDOWN_MS));
  assert(ping_probe_due_at(1000, 1000 + COOLDOWN_MS + 1, COOLDOWN_MS));
}

static void test_tick_wraparound(void) {
  /* Tick counter near rollover: probed just before UINT32_MAX, now just after
   * the wrap. Unsigned subtraction yields the true (small) elapsed, so we're
   * still cooling down -> not due. A naive signed compare would treat the wrap
   * as a huge elapsed and re-probe, reopening the flood across the ~49-day
   * dc_get_ticks() rollover. */
  uint32_t last = UINT32_MAX - 100u;
  assert(!ping_probe_due_at(last, last + 50u, COOLDOWN_MS)); /* elapsed 50 */
  /* And once the interval truly elapses across the wrap, it's due again. */
  assert(ping_probe_due_at(last, last + COOLDOWN_MS, COOLDOWN_MS));
}

_MAIN_HEAD_(void) argc;
(void)argv;
test_never_probed_is_due();
test_within_cooldown_not_due();
test_just_probed_not_due();
test_cooldown_boundary_is_due();
test_tick_wraparound();
fprintf(stderr, "ping-probe cooldown tests: OK\n");
_MAIN_TAIL_
