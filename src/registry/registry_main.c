/*
 registry_main.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Initial version written by Claude (Opus 4.8, an LLM by Anthropic) at Andy's
 direction.
*/

/*
  Dealer's Choice server registry (directory) daemon. Game servers announce
  themselves over TCP; clients query the list. The registry callback-verifies
  each advertised server speaks the DC protocol before listing it, expires
  stale entries (TTL), caps listings per source IP, and writes a servers.json
  the website can serve. Links libdc_core only (no SDL).
*/

#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dc_time.h"
#include "net.h"
#include "registry.h"
#include "util.h"

/* Registry policy (DC-owned; tcpme stays generic). */
#define REG_TTL_MS 90000u    /* drop a listing not refreshed within this */
#define REG_PER_IP_MAX 4     /* max distinct listings per source IP */
/* Verify is a blocking connect-back on this single-threaded loop, so it is kept
 * short: every announce of an unreachable address (scanner, or a real server
 * behind NAT) costs the full timeout, during which no list request is served. */
#define REG_VERIFY_TIMEOUT_MS 500u /* callback-verify connect/handshake timeout */
#define REG_IO_TIMEOUT_MS 2000u    /* per-connection recv (bounds a slow peer) */
#define REG_VERIFY_FAIL_BACKOFF_MS 30000u /* skip re-verifying a just-failed IP */
#define REG_FAIL_TRACK 64                 /* recently-failed source IPs tracked */
#define REG_JSON_INTERVAL_MS 5000u        /* how often to rewrite servers.json */
#define REG_TICK_MS 500u                  /* accept-poll tick */

typedef struct {
  RegistryServer_t srv;
  uint32_t last_seen;
  bool used;
} RegEntry_t;

static RegEntry_t g_table[REGISTRY_MAX_SERVERS];
static bool g_verbose = false;
static volatile sig_atomic_t g_running = 1;

/* Recently-failed source IPs. After a verify failure we skip re-verifying that
 * IP for REG_VERIFY_FAIL_BACKOFF_MS, so a host announcing an unreachable address
 * (scanner, or a real server behind NAT) can't make every heartbeat cost a full
 * verify timeout on this single-threaded loop. Keyed on IP only, so a flooder
 * that varies the announced port is still covered; only failures are tracked, so
 * legit servers (which verify fine, even several behind one NAT'd IP) are never
 * throttled. */
typedef struct {
  char ip[TCPME_ADDRSTRLEN];
  uint32_t at;
  bool used;
} FailIp_t;
static FailIp_t g_fail[REG_FAIL_TRACK];

static void on_signal(int sig) {
  (void)sig;
  g_running = 0;
}

static void rlog(const char *fmt, ...) {
  if (!g_verbose)
    return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

/* Confirm the advertised server actually speaks the DC protocol by connecting
 * back and doing the version handshake (no password needed). Reuses the game
 * port, so it needs no extra firewall opening on the server's side. */
static bool callback_verify(const char *ip, uint16_t port) {
  tcpme_socket_t s = tcpme_connect_timeout(ip, port, REG_VERIFY_TIMEOUT_MS);
  if (!tcpme_socket_valid(s))
    return false;
  tcpme_set_timeout(s, REG_VERIFY_TIMEOUT_MS);
  bool ok = (send_protocol_header(s, PROTO_FLAG_PROBE) == 0);
  tcpme_close(s);
  return ok;
}

static int count_for_ip(const char *ip) {
  int n = 0;
  for (int i = 0; i < REGISTRY_MAX_SERVERS; i++)
    if (g_table[i].used && strcmp(g_table[i].srv.ip, ip) == 0)
      n++;
  return n;
}

/* Insert or refresh, keyed on ip+port. Returns false if a per-IP cap or the
 * table capacity is hit (refreshing an existing entry always succeeds). */
static bool table_upsert(const RegistryServer_t *srv, uint32_t now) {
  for (int i = 0; i < REGISTRY_MAX_SERVERS; i++) {
    if (g_table[i].used && g_table[i].srv.tcp_port == srv->tcp_port &&
        strcmp(g_table[i].srv.ip, srv->ip) == 0) {
      g_table[i].srv = *srv;
      g_table[i].last_seen = now;
      return true;
    }
  }
  if (count_for_ip(srv->ip) >= REG_PER_IP_MAX)
    return false;
  for (int i = 0; i < REGISTRY_MAX_SERVERS; i++) {
    if (!g_table[i].used) {
      g_table[i].srv = *srv;
      g_table[i].last_seen = now;
      g_table[i].used = true;
      return true;
    }
  }
  return false;
}

static void table_expire(uint32_t now) {
  for (int i = 0; i < REGISTRY_MAX_SERVERS; i++)
    if (g_table[i].used && now - g_table[i].last_seen > REG_TTL_MS)
      g_table[i].used = false;
}

static int table_snapshot(RegistryServer_t *out, int max) {
  int n = 0;
  for (int i = 0; i < REGISTRY_MAX_SERVERS && n < max; i++)
    if (g_table[i].used)
      out[n++] = g_table[i].srv;
  return n;
}

static void json_escape(const char *s, char *out, size_t outsz) {
  size_t j = 0;
  for (size_t i = 0; s[i] && j + 2 < outsz; i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      out[j++] = '\\';
      out[j++] = c;
    } else {
      out[j++] = c;
    }
  }
  out[j] = '\0';
}

/* Atomic write of the current list as a JSON array (temp + rename). */
static void write_servers_json(const char *path) {
  char tmp[1024];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *fp = fopen(tmp, "w");
  if (!fp) {
    perror("registry: open servers.json temp");
    return;
  }
  fputs("[\n", fp);
  bool first = true;
  for (int i = 0; i < REGISTRY_MAX_SERVERS; i++) {
    if (!g_table[i].used)
      continue;
    char ename[REGISTRY_NAME_MAX * 2 + 1];
    json_escape(g_table[i].srv.name, ename, sizeof ename);
    /* start_time is the server's reported wall-clock boot (unix seconds, 0 =
     * unknown). Emit it raw so the website can compute uptime against its own
     * "now" rather than baking a stale value into the file. */
    fprintf(fp,
            "%s  {\"ip\":\"%s\",\"port\":%u,\"name\":\"%s\",\"players\":%u,"
            "\"max\":%u,\"password\":%s,\"in_progress\":%s,\"start_time\":%llu}",
            first ? "" : ",\n", g_table[i].srv.ip, g_table[i].srv.tcp_port, ename,
            g_table[i].srv.player_count, g_table[i].srv.max_players,
            g_table[i].srv.password_protected ? "true" : "false",
            g_table[i].srv.in_progress ? "true" : "false",
            (unsigned long long)g_table[i].srv.start_time);
    first = false;
  }
  fputs("\n]\n", fp);
  if (fclose(fp) != 0) {
    perror("registry: write servers.json");
    return;
  }
  if (rename(tmp, path) != 0)
    perror("registry: rename servers.json");
}

/* Strip the IPv4-mapped IPv6 prefix so a plain IPv4 server shows as "1.2.3.4"
 * rather than "::ffff:1.2.3.4" (the form a dual-stack listener reports). */
static const char *normalize_ip(const char *ip) {
  if (strncmp(ip, "::ffff:", 7) == 0 && strchr(ip + 7, '.'))
    return ip + 7;
  return ip;
}

/* True if this IP failed verify within the backoff window — skip the (blocking)
 * re-verify and reject the announce cheaply. */
static bool verify_backing_off(const char *ip, uint32_t now) {
  for (int i = 0; i < REG_FAIL_TRACK; i++)
    if (g_fail[i].used && strcmp(g_fail[i].ip, ip) == 0)
      return (now - g_fail[i].at) < REG_VERIFY_FAIL_BACKOFF_MS;
  return false;
}

/* Record (or refresh) a verify failure for this IP. Reuses an existing slot for
 * the same IP, else a free slot, else evicts the oldest. */
static void note_verify_fail(const char *ip, uint32_t now) {
  int slot = -1;
  for (int i = 0; i < REG_FAIL_TRACK; i++) {
    if (g_fail[i].used && strcmp(g_fail[i].ip, ip) == 0) {
      slot = i;
      break;
    }
    if (!g_fail[i].used && slot < 0)
      slot = i;
  }
  if (slot < 0) {
    slot = 0;
    for (int i = 1; i < REG_FAIL_TRACK; i++)
      if (g_fail[i].at < g_fail[slot].at)
        slot = i;
  }
  snprintf(g_fail[slot].ip, sizeof g_fail[slot].ip, "%s", ip);
  g_fail[slot].at = now;
  g_fail[slot].used = true;
}

/* Clear any failure record for an IP that has now verified successfully. */
static void clear_verify_fail(const char *ip) {
  for (int i = 0; i < REG_FAIL_TRACK; i++)
    if (g_fail[i].used && strcmp(g_fail[i].ip, ip) == 0)
      g_fail[i].used = false;
}

static void handle_connection(tcpme_socket_t client, const char *peer_ip) {
  uint16_t opcode = 0;
  const uint8_t *payload = NULL;
  size_t plen = 0;
  uint8_t *frame = registry_recv_frame(client, &opcode, &payload, &plen);
  if (!frame)
    return;

  if (opcode == MSG_REG_ANNOUNCE) {
    RegistryServer_t srv;
    if (registry_parse_announce(payload, plen, &srv) == 0) {
      /* Trust the connection source for the IP, never the payload. */
      snprintf(srv.ip, sizeof srv.ip, "%s", normalize_ip(peer_ip));
      uint32_t now = dc_get_ticks();
      if (srv.tcp_port == 0) {
        /* nothing to verify or list */
      } else if (verify_backing_off(srv.ip, now)) {
        rlog("skip verify (backoff) %s:%u", srv.ip, srv.tcp_port);
      } else if (callback_verify(srv.ip, srv.tcp_port)) {
        clear_verify_fail(srv.ip);
        if (table_upsert(&srv, now))
          rlog("listed %s:%u \"%s\" (%u/%u)", srv.ip, srv.tcp_port, srv.name, srv.player_count,
               srv.max_players);
        else
          rlog("rejected %s:%u (per-IP cap or full)", srv.ip, srv.tcp_port);
      } else {
        note_verify_fail(srv.ip, now);
        rlog("verify failed %s:%u", srv.ip, srv.tcp_port);
      }
    }
  } else if (opcode == MSG_REG_LIST_REQUEST) {
    RegistryServer_t list[REGISTRY_MAX_SERVERS];
    int n = table_snapshot(list, REGISTRY_MAX_SERVERS);
    registry_send_list(client, list, n);
    rlog("served list (%d servers) to %s", n, peer_ip);
  }

  free(frame);
}

static int run_registry(uint16_t port, const char *json_path) {
  if (tcpme_init() != 0) {
    fputs("registry: tcpme_init failed\n", stderr);
    return 1;
  }
  tcpme_socket_t server = tcpme_listen(NULL, port);
  if (!tcpme_socket_valid(server)) {
    fprintf(stderr, "registry: listen on %u failed: %s\n", port, tcpme_get_error());
    tcpme_quit();
    return 1;
  }
  tcpme_set_t *set = tcpme_alloc_set(1);
  if (!set) {
    tcpme_close(server);
    tcpme_quit();
    return 1;
  }
  tcpme_add_socket(set, server);

  printf("Dealer's Choice registry listening on port %u\n", port);
  if (json_path)
    printf("Writing server list to %s\n", json_path);
  fflush(stdout);

  uint32_t last_json = 0;
  while (g_running) {
    tcpme_check_sockets(set, REG_TICK_MS);
    if (tcpme_socket_ready(set, server)) {
      tcpme_socket_t client = tcpme_accept(server);
      if (tcpme_socket_valid(client)) {
        tcpme_set_timeout(client, REG_IO_TIMEOUT_MS);
        char peer_ip[TCPME_ADDRSTRLEN] = {0};
        tcpme_get_peer_ip(client, peer_ip, sizeof peer_ip);
        handle_connection(client, peer_ip);
        tcpme_close(client);
      }
    }
    uint32_t now = dc_get_ticks();
    table_expire(now);
    if (json_path && now - last_json >= REG_JSON_INTERVAL_MS) {
      write_servers_json(json_path);
      last_json = now;
    }
  }

  printf("registry: shutting down\n");
  tcpme_free_set(set);
  tcpme_close(server);
  tcpme_quit();
  return 0;
}

static void usage(const char *argv0) {
  printf("Usage: %s [options]\n"
         "  --port <port>   listen port (default %s)\n"
         "  --json <path>   write the server list as JSON to <path>\n"
         "  --verbose       log announces / list requests\n"
         "  --help          this help\n",
         argv0, REGISTRY_DEFAULT_PORT);
}

int main(int argc, char *argv[]) {
  uint16_t port = (uint16_t)atoi(REGISTRY_DEFAULT_PORT);
  const char *json_path = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      unsigned long pv;
      parse_unsigned(argv[++i], UINT16_MAX, &pv);
      port = (uint16_t)pv;
    } else if (strcmp(argv[i], "--json") == 0 && i + 1 < argc) {
      json_path = argv[++i];
    } else if (strcmp(argv[i], "--verbose") == 0) {
      g_verbose = true;
    } else if (strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
      usage(argv[0]);
      return 1;
    }
  }

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  return run_registry(port, json_path);
}
