/*
 ping_probe.h
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2026 Andy Alt
 This file was written by Claude (Opus 4.8) at Andy's direction.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

/* Background latency prober for the connect-screen server list.
 *
 * Measures each server's latency as the wall-clock time of a TCP connect to
 * its host:port (~1 round trip). This is a *proxy* for latency, not a true
 * ping: it includes the TCP three-way-handshake cost and the server's
 * accept() scheduling, and it opens (then immediately closes) a real
 * connection each cycle. A dedicated protocol PingRequest/PingResponse over an
 * already-open socket would be more accurate, but that needs a pre-join
 * handshake and a wire-protocol addition; connect-RTT needs neither and is
 * good enough to rank servers by responsiveness.
 *
 * All the real work (the blocking connects) happens on a background thread so
 * the ~60fps connect-screen loop never stalls. The UI thread feeds the current
 * server list in with ping_probe_set_targets() and reads results back with
 * ping_probe_get(); both are cheap, mutex-guarded, and safe to call every
 * frame. Results are keyed by "host:port" string, so they survive the list
 * being reordered or partially rebuilt between probe cycles.
 */

#ifndef __PING_PROBE_H
#define __PING_PROBE_H

#include <stdbool.h>
#include <stdint.h>

/* Result sentinels stored per slot (otherwise the value is a latency in ms). */
enum {
  PING_PENDING = -1,     /* not measured yet this cycle */
  PING_UNREACHABLE = -2, /* connect failed or timed out */
};

typedef struct PingProbe PingProbe_t;

/* Create and start the background prober. Returns NULL on failure (in which
 * case the caller simply shows no ping data). */
PingProbe_t *ping_probe_create(void);

/* Signal the thread to stop, join it, and free everything. Safe with NULL. */
void ping_probe_destroy(PingProbe_t *p);

/* Replace the set of targets to probe. `hosts`/`ports` are parallel arrays of
 * length `count`; the strings are copied, so the caller's buffers need not
 * outlive the call. Targets whose host:port match the previous set keep their
 * last measured value (so a result doesn't flicker back to "-" on every list
 * rebuild); new targets start at PING_PENDING. Setting targets also wakes the
 * thread to probe promptly rather than waiting out the current sleep. */
void ping_probe_set_targets(PingProbe_t *p, const char *const *hosts, const uint16_t *ports,
                            int count);

/* Look up the latest result for host:port. Returns a latency in ms, or one of
 * the PING_* sentinels. Unknown targets return PING_PENDING. */
int ping_probe_get(PingProbe_t *p, const char *host, uint16_t port);

/* True if a target last probed at last_probe_ticks (0 = never probed) is due to
 * be probed again at now_ticks, given a cooldown of interval_ms. This is the
 * per-target rate cap that stops a flapping UI list from re-probing — and
 * flooding — a server faster than once per cooldown. Pure and time-injected so
 * the cap is unit-testable without spinning the worker thread; the worker calls
 * it with dc_get_ticks() and the internal cycle interval. */
bool ping_probe_due_at(uint32_t last_probe_ticks, uint32_t now_ticks, uint32_t interval_ms);

#endif
