/*
 registry_fetch_thread.c
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
#include "registry_fetch_thread.h"
#include "tcpme/tcpme.h"
#include "util.h"

/* Gap between fetch cycles. Matches the connect screen's previous ~10s registry
 * refresh cadence so behaviour is preserved. */
enum { REG_CYCLE_MS = 10000 };

/* How finely the inter-cycle sleep is sliced, so a stop request is noticed
 * within this long rather than after a full cycle. */
enum { REG_SLEEP_SLICE_MS = 50 };

/* Timeouts for browsing a registry. These cover a full INTERNET round trip (DNS
 * resolution + TCP handshake to a remote registry), so they must be generous:
 * 300ms was fine on LAN/localhost but timed out for remote users over
 * higher-latency links / first-time DNS lookups. They run on this worker thread
 * (not the UI thread), so a slow/unreachable registry costs only worker time. */
#define REG_FETCH_CONNECT_MS 2500
#define REG_FETCH_IO_MS 2000

struct RegistryFetcher {
  SDL_Thread *thread;
  SDL_mutex *lock; /* guards the published buffer below */
  SDL_atomic_t stop;

  /* Snapshot of the registries to browse, taken at create time. Read-only on
   * the worker after creation, so no lock is needed for it. */
  char registry_host[MAX_REGISTRIES][REGISTRY_HOST_LEN];
  uint16_t registry_port[MAX_REGISTRIES];
  uint8_t registry_count;

  /* The published result. The worker writes it under `lock`; the UI thread
   * reads a copy under `lock`. `version` bumps on every completed fetch so the
   * reader can tell a new result from the last one it saw (0 = no fetch yet). */
  RegistryServer_t published[REGISTRY_MAX_SERVERS];
  int published_count;
  uint32_t version;
};

/* Query every snapshotted registry and merge the results (dedup by ip:port)
 * into out[]. Bounded connect/recv so an unreachable registry can't hang.
 * Returns the count. Runs on the worker thread with the lock NOT held (this is
 * the slow part). */
static int registry_fetch_blocking(RegistryFetcher_t *f, RegistryServer_t *out, int max) {
  int count = 0;
  for (int r = 0; r < f->registry_count && count < max; r++) {
    if (SDL_AtomicGet(&f->stop))
      break;
    tcpme_socket_t s =
        tcpme_connect_timeout(f->registry_host[r], f->registry_port[r], REG_FETCH_CONNECT_MS);
    if (!tcpme_socket_valid(s))
      continue;
    tcpme_set_timeout(s, REG_FETCH_IO_MS);
    RegistryServer_t list[REGISTRY_MAX_SERVERS];
    int n = 0;
    if (registry_send_list_request(s) == 0 &&
        registry_recv_list(s, list, REGISTRY_MAX_SERVERS, &n) == 0) {
      for (int i = 0; i < n && count < max; i++) {
        bool dup = false;
        for (int j = 0; j < count; j++)
          if (out[j].tcp_port == list[i].tcp_port && strcmp(out[j].ip, list[i].ip) == 0) {
            dup = true;
            break;
          }
        if (!dup)
          out[count++] = list[i];
      }
    }
    tcpme_close(s);
  }
  return count;
}

static int registry_thread_fn(void *data) {
  RegistryFetcher_t *f = data;

  while (!SDL_AtomicGet(&f->stop)) {
    /* Do the slow fetch into a local buffer with the lock released, then publish
     * the finished result under the lock in one short critical section. */
    RegistryServer_t local[REGISTRY_MAX_SERVERS];
    int n = registry_fetch_blocking(f, local, REGISTRY_MAX_SERVERS);

    if (SDL_AtomicGet(&f->stop))
      break;

    SDL_LockMutex(f->lock);
    memcpy(f->published, local, sizeof(RegistryServer_t) * (size_t)n);
    f->published_count = n;
    f->version++; /* even n == 0 counts: the reader learns the fetch finished */
    SDL_UnlockMutex(f->lock);

    dc_log(DC_LOG_DEBUG, "registry: fetched %d server(s)", n);
    for (int i = 0; i < n; i++) {
      char addr[TCPME_ADDRSTRLEN + 8];
      registry_format_addr(addr, sizeof addr, local[i].ip, local[i].tcp_port);
      dc_log(DC_LOG_DEBUG, "registry:   [%d] %s \"%s\" %u/%u", i, addr, local[i].name,
             (unsigned)local[i].player_count, (unsigned)local[i].max_players);
    }

    /* Sleep until the next cycle, in slices, breaking early on a stop request so
     * the join in registry_fetch_destroy() returns promptly. */
    uint32_t slept = 0;
    while (slept < REG_CYCLE_MS) {
      if (SDL_AtomicGet(&f->stop))
        return 0;
      dc_sleep_ms(REG_SLEEP_SLICE_MS);
      slept += REG_SLEEP_SLICE_MS;
    }
  }
  return 0;
}

RegistryFetcher_t *registry_fetch_create(const PlayerConfig_t *pc) {
  if (!pc)
    return NULL;
  RegistryFetcher_t *f = calloc(1, sizeof(*f));
  if (!f)
    return NULL;

  /* Snapshot the registry set so the worker never reads the caller's config
   * concurrently. */
  f->registry_count = pc->registry_count;
  for (int r = 0; r < pc->registry_count && r < MAX_REGISTRIES; r++) {
    snprintf(f->registry_host[r], sizeof(f->registry_host[r]), "%s", pc->registry_host[r]);
    f->registry_port[r] = pc->registry_port[r];
  }

  f->lock = SDL_CreateMutex();
  if (!f->lock) {
    free(f);
    return NULL;
  }
  SDL_AtomicSet(&f->stop, 0);
  f->thread = SDL_CreateThread(registry_thread_fn, "registry_fetch", f);
  if (!f->thread) {
    SDL_DestroyMutex(f->lock);
    free(f);
    return NULL;
  }
  return f;
}

void registry_fetch_destroy(RegistryFetcher_t *f) {
  if (!f)
    return;
  /* Signal stop and join. Worst-case join latency is one in-flight fetch (the
   * blocking loop checks `stop` between registries) plus one sleep slice. */
  SDL_AtomicSet(&f->stop, 1);
  SDL_WaitThread(f->thread, NULL);
  SDL_DestroyMutex(f->lock);
  free(f);
}

uint32_t registry_fetch_get(RegistryFetcher_t *f, RegistryServer_t *out, int max, int *count) {
  if (count)
    *count = 0;
  if (!f || !out || max <= 0)
    return 0;

  SDL_LockMutex(f->lock);
  uint32_t version = f->version;
  int n = f->published_count;
  if (n > max)
    n = max;
  memcpy(out, f->published, sizeof(RegistryServer_t) * (size_t)n);
  SDL_UnlockMutex(f->lock);

  if (count)
    *count = n;
  return version;
}
