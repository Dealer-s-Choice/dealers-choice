/* ping_probe_flood.c
 *
 * Regression check for the connect-screen ping-probe storm: when the server
 * list "flaps", the UI re-pushes the probe's target set many times a second.
 * Pre-fix, every push forced a re-probe, flooding each listed server with
 * connections and tripping its per-IP connection rate limit (so the user's own
 * real connect got rejected). The per-target cooldown caps a server to ~one
 * probe per cycle no matter how often the list is pushed.
 *
 * This drives the REAL PingProbe against a loopback listener that counts
 * incoming connections, pushing targets every ~16ms for a window longer than
 * one cooldown. Pass = a trickle (cooldown holds); a flood would be hundreds.
 */
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#include "dc_time.h"
#include "ping_probe.h"
#include "tcpme/tcpme.h"

static tcpme_socket_t g_listener;
static SDL_atomic_t g_accepts;
static SDL_atomic_t g_stop;

static int accept_thread(void *unused) {
  (void)unused;
  while (!SDL_AtomicGet(&g_stop)) {
    tcpme_socket_t c = tcpme_accept(g_listener);
    if (tcpme_socket_valid(c)) {
      SDL_AtomicAdd(&g_accepts, 1);
      char buf[64];
      tcpme_recv(c, buf, sizeof buf); /* drain the probe's PROBE header */
      tcpme_close(c);
    } else {
      dc_sleep_ms(5);
    }
  }
  return 0;
}

/* int main(int, char**) — not (void): on Windows SDL's SDL_main.h does
 * `#define main SDL_main` and declares SDL_main(int, char**), so a (void) main
 * conflicts. Matches the signature the _MAIN_HEAD_ test harness uses. */
int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  if (tcpme_init() != 0) {
    fprintf(stderr, "tcpme_init failed\n");
    return 1;
  }
  g_listener = tcpme_listen("127.0.0.1", 0); /* ephemeral loopback port */
  if (!tcpme_socket_valid(g_listener)) {
    fprintf(stderr, "listen failed: %s\n", tcpme_get_error());
    return 1;
  }
  char addr[TCPME_ADDRPORTSTRLEN];
  if (!tcpme_get_local_addr(g_listener, addr, sizeof addr)) {
    fprintf(stderr, "get_local_addr failed\n");
    return 1;
  }
  const char *colon = strrchr(addr, ':');
  uint16_t port = colon ? (uint16_t)strtoul(colon + 1, NULL, 10) : 0;
  printf("listener: %s\n", addr);

  SDL_AtomicSet(&g_accepts, 0);
  SDL_AtomicSet(&g_stop, 0);
  SDL_Thread *acc = SDL_CreateThread(accept_thread, "acc", NULL);

  PingProbe_t *p = ping_probe_create();
  if (!p) {
    fprintf(stderr, "ping_probe_create failed\n");
    return 1;
  }

  /* Re-push the (flapping) target list every frame for the window. */
  const char *hosts[1] = {"127.0.0.1"};
  uint16_t ports[1] = {port};
  const uint32_t window_ms = 12000; /* > one 10s cooldown -> expect ~2 probes */
  uint32_t t0 = dc_get_ticks();
  int pushes = 0;
  while (dc_get_ticks() - t0 < window_ms) {
    ping_probe_set_targets(p, hosts, ports, 1);
    pushes++;
    dc_sleep_ms(16); /* ~60 fps */
  }

  ping_probe_destroy(p);
  SDL_AtomicSet(&g_stop, 1);
  SDL_WaitThread(acc, NULL);
  tcpme_close(g_listener);
  tcpme_quit();

  int accepts = SDL_AtomicGet(&g_accepts);
  printf("window=%ums pushes=%d probe-connections=%d\n", window_ms, pushes, accepts);
  /* Cooldown is 10s, so a 12s window allows ~2 probes plus startup slack. */
  if (accepts <= 4) {
    printf("PASS: no flood (%d connections for ~%d list pushes)\n", accepts, pushes);
    return 0;
  }
  printf("FAIL: flood reproduced (%d connections)\n", accepts);
  return 1;
}
