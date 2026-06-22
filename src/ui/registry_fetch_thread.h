/*
 registry_fetch_thread.h
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

/* Background registry fetcher for the connect screen's "Internet servers" list.
 *
 * Querying a registry is blocking work: a TCP connect (REG_FETCH_CONNECT_MS)
 * plus a list-request/response round trip (REG_FETCH_IO_MS) per configured
 * registry. Done inline on the ~60fps connect-screen loop (the old behaviour),
 * one slow or unreachable registry froze the window for several seconds (#82).
 *
 * This moves that work onto a dedicated worker thread, modelled on ping_probe:
 * the worker fetches on an interval (~10s) and on demand, then publishes the
 * merged RegistryServer_t list into a mutex-guarded buffer. The UI thread reads
 * the latest published list each frame with registry_fetch_get() (cheap,
 * mutex-guarded copy) and rebuilds its table only when the result version
 * changes. The blocking fetch never runs on the UI thread.
 *
 * It is a second, independent worker alongside ping_probe on this screen; the
 * two share no state. The registry set is snapshotted from the PlayerConfig_t
 * at create time (registries are fixed for the screen's lifetime), so the worker
 * never touches the caller's config concurrently.
 */

#ifndef __REGISTRY_FETCH_THREAD_H
#define __REGISTRY_FETCH_THREAD_H

#include <stdint.h>

#include "dc_config.h"
#include "net/registry.h"

typedef struct RegistryFetcher RegistryFetcher_t;

/* Create and start the background fetcher, snapshotting the registries to browse
 * from `pc`. Returns NULL on failure (caller then shows no registry rows). The
 * first fetch runs promptly; until it completes, registry_fetch_get() reports
 * version 0 with no servers. */
RegistryFetcher_t *registry_fetch_create(const PlayerConfig_t *pc);

/* Signal the worker to stop, join it, and free everything. Safe with NULL. */
void registry_fetch_destroy(RegistryFetcher_t *f);

/* Copy the latest published list into out[0..max-1], setting *count. Returns a
 * monotonically increasing version that bumps on every completed fetch (even one
 * that yields zero servers), so the caller can rebuild its table only when the
 * version changes. Returns 0 before the first fetch completes (and leaves
 * *count = 0). Safe to call every frame. */
uint32_t registry_fetch_get(RegistryFetcher_t *f, RegistryServer_t *out, int max, int *count);

#endif
