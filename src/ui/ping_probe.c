/*
 ping_probe.c
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

#include <SDL.h>
#include <stdlib.h>
#include <string.h>

#include "dc_time.h"
#include "ping_probe.h"
#include "tcpme/tcpme.h"
#include "util.h"

/* Upper bound on tracked servers: the connect screen shows at most
 * REG_MAX_SHOWN (8) registry + LAN_MAX_SHOWN (6) LAN rows = 14. Round up so a
 * future cap bump has headroom without reallocating. */
enum { PING_MAX_TARGETS = 32 };

/* Per-target connect timeout. Long enough to ride out a real WAN handshake,
 * short enough that one dead server doesn't stall a whole probe cycle for
 * long. A dead host marks PING_UNREACHABLE after this. */
enum { PING_CONNECT_TIMEOUT_MS = 1500 };

/* Gap between probe cycles. Matches the connect screen's ~10s registry refresh
 * so the two stay roughly in step. */
enum { PING_CYCLE_MS = 10000 };

/* How finely the inter-cycle sleep is sliced, so stop/wake requests are noticed
 * within this long rather than after a full cycle. */
enum { PING_SLEEP_SLICE_MS = 50 };

typedef struct {
  char host[TCPME_ADDRSTRLEN];
  uint16_t port;
  int ms; /* latency, or a PING_* sentinel */
} PingTarget_t;

struct PingProbe {
  SDL_Thread *thread;
  SDL_mutex *lock; /* guards everything below */
  SDL_atomic_t stop;

  /* The shared target/result table. The UI thread writes targets; the worker
   * writes results; both under `lock`. */
  PingTarget_t targets[PING_MAX_TARGETS];
  int count;

  /* Bumped by the UI thread whenever it sets new targets, so the worker can
   * tell its snapshot is stale and re-probe promptly instead of sleeping. */
  uint32_t generation;
};

/* Find target i matching host:port, or -1. Caller holds the lock. */
static int find_target(const PingProbe_t *p, const char *host, uint16_t port) {
  for (int i = 0; i < p->count; i++)
    if (p->targets[i].port == port && strcmp(p->targets[i].host, host) == 0)
      return i;
  return -1;
}

/* Probe one host:port: time a connect, then close. Returns ms or a sentinel.
 * Runs on the worker thread with the lock NOT held (this is the slow part). */
static int probe_one(const char *host, uint16_t port) {
  uint32_t t0 = dc_get_ticks();
  tcpme_socket_t s = tcpme_connect_timeout(host, port, PING_CONNECT_TIMEOUT_MS);
  uint32_t elapsed = dc_get_ticks() - t0;
  if (!tcpme_socket_valid(s))
    return PING_UNREACHABLE;
  tcpme_close(s);
  /* Connect-RTT can round to 0 on loopback; report at least 1ms so the cell
   * never shows a misleading "0" that looks like "no data". */
  return elapsed > 0 ? (int)elapsed : 1;
}

static int ping_thread_fn(void *data) {
  PingProbe_t *p = data;

  while (!SDL_AtomicGet(&p->stop)) {
    /* Snapshot the current targets under the lock, then probe each with the
     * lock released so the slow connects never block the UI thread's
     * set_targets()/get() calls. */
    PingTarget_t snap[PING_MAX_TARGETS];
    int n;
    uint32_t gen;
    SDL_LockMutex(p->lock);
    n = p->count;
    memcpy(snap, p->targets, sizeof(PingTarget_t) * (size_t)n);
    gen = p->generation;
    SDL_UnlockMutex(p->lock);

    for (int i = 0; i < n; i++) {
      if (SDL_AtomicGet(&p->stop))
        return 0;
      int ms = probe_one(snap[i].host, snap[i].port);

      /* Write the result back, but only if this exact target still exists
       * (the UI thread may have replaced the list while we were connecting).
       * Match by host:port rather than index, since the list can reorder. */
      SDL_LockMutex(p->lock);
      int idx = find_target(p, snap[i].host, snap[i].port);
      if (idx >= 0)
        p->targets[idx].ms = ms;
      SDL_UnlockMutex(p->lock);
    }

    /* Sleep until the next cycle, in slices, breaking early if asked to stop
     * or if the UI handed us a new target set (generation changed) so fresh
     * servers get a quick first measurement. */
    uint32_t slept = 0;
    while (slept < PING_CYCLE_MS) {
      if (SDL_AtomicGet(&p->stop))
        return 0;
      SDL_LockMutex(p->lock);
      bool changed = (p->generation != gen);
      SDL_UnlockMutex(p->lock);
      if (changed)
        break;
      dc_sleep_ms(PING_SLEEP_SLICE_MS);
      slept += PING_SLEEP_SLICE_MS;
    }
  }
  return 0;
}

PingProbe_t *ping_probe_create(void) {
  PingProbe_t *p = calloc(1, sizeof(*p));
  if (!p)
    return NULL;
  p->lock = SDL_CreateMutex();
  if (!p->lock) {
    free(p);
    return NULL;
  }
  SDL_AtomicSet(&p->stop, 0);
  p->thread = SDL_CreateThread(ping_thread_fn, "ping_probe", p);
  if (!p->thread) {
    SDL_DestroyMutex(p->lock);
    free(p);
    return NULL;
  }
  return p;
}

void ping_probe_destroy(PingProbe_t *p) {
  if (!p)
    return;
  /* Signal stop and join. The worst-case join latency is one connect timeout
   * (a probe in flight) plus one sleep slice, since the worker checks `stop`
   * between every probe and on every slice. */
  SDL_AtomicSet(&p->stop, 1);
  SDL_WaitThread(p->thread, NULL);
  SDL_DestroyMutex(p->lock);
  free(p);
}

void ping_probe_set_targets(PingProbe_t *p, const char *const *hosts, const uint16_t *ports,
                            int count) {
  if (!p)
    return;
  if (count > PING_MAX_TARGETS)
    count = PING_MAX_TARGETS;

  SDL_LockMutex(p->lock);
  PingTarget_t next[PING_MAX_TARGETS];
  int nc = 0;
  for (int i = 0; i < count; i++) {
    if (!hosts[i] || hosts[i][0] == '\0')
      continue;
    /* Carry over a prior measurement for the same host:port so the value
     * doesn't flicker to "-" every time the UI rebuilds its list. */
    int prev = find_target(p, hosts[i], ports[i]);
    snprintf(next[nc].host, sizeof(next[nc].host), "%s", hosts[i]);
    next[nc].port = ports[i];
    next[nc].ms = (prev >= 0) ? p->targets[prev].ms : PING_PENDING;
    nc++;
  }
  memcpy(p->targets, next, sizeof(PingTarget_t) * (size_t)nc);
  p->count = nc;
  p->generation++; /* nudge the worker to re-probe promptly */
  SDL_UnlockMutex(p->lock);
}

int ping_probe_get(PingProbe_t *p, const char *host, uint16_t port) {
  if (!p || !host)
    return PING_PENDING;
  SDL_LockMutex(p->lock);
  int idx = find_target(p, host, port);
  int ms = (idx >= 0) ? p->targets[idx].ms : PING_PENDING;
  SDL_UnlockMutex(p->lock);
  return ms;
}
