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

/* Threading portability for the verify worker. The callback-verify is a
 * blocking connect-back, so it runs on a dedicated worker thread instead of the
 * accept loop (one slow/unreachable announcer would otherwise stall every other
 * client for the full verify timeout). pthreads elsewhere, Win32 on Windows --
 * the same split tcpme uses. Only the registry uses threads, so the shim is kept
 * local rather than promoted to a shared header. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef HANDLE reg_thread_t;
typedef CRITICAL_SECTION reg_mutex_t;
typedef CONDITION_VARIABLE reg_cond_t;
#define REG_THREAD_FN DWORD WINAPI
#define REG_THREAD_RET 0
static int reg_thread_create(reg_thread_t *t, LPTHREAD_START_ROUTINE fn, void *arg) {
  *t = CreateThread(NULL, 0, fn, arg, 0, NULL);
  return *t ? 0 : -1;
}
static void reg_thread_join(reg_thread_t t) {
  WaitForSingleObject(t, INFINITE);
  CloseHandle(t);
}
static void reg_mutex_init(reg_mutex_t *m) { InitializeCriticalSection(m); }
static void reg_mutex_destroy(reg_mutex_t *m) { DeleteCriticalSection(m); }
static void reg_mutex_lock(reg_mutex_t *m) { EnterCriticalSection(m); }
static void reg_mutex_unlock(reg_mutex_t *m) { LeaveCriticalSection(m); }
static void reg_cond_init(reg_cond_t *c) { InitializeConditionVariable(c); }
static void reg_cond_destroy(reg_cond_t *c) { (void)c; }
static void reg_cond_signal(reg_cond_t *c) { WakeConditionVariable(c); }
static void reg_cond_wait(reg_cond_t *c, reg_mutex_t *m) {
  SleepConditionVariableCS(c, m, INFINITE);
}
#else
#include <pthread.h>
typedef pthread_t reg_thread_t;
typedef pthread_mutex_t reg_mutex_t;
typedef pthread_cond_t reg_cond_t;
#define REG_THREAD_FN void *
#define REG_THREAD_RET NULL
static int reg_thread_create(reg_thread_t *t, void *(*fn)(void *), void *arg) {
  return pthread_create(t, NULL, fn, arg);
}
static void reg_thread_join(reg_thread_t t) { pthread_join(t, NULL); }
static void reg_mutex_init(reg_mutex_t *m) { pthread_mutex_init(m, NULL); }
static void reg_mutex_destroy(reg_mutex_t *m) { pthread_mutex_destroy(m); }
static void reg_mutex_lock(reg_mutex_t *m) { pthread_mutex_lock(m); }
static void reg_mutex_unlock(reg_mutex_t *m) { pthread_mutex_unlock(m); }
static void reg_cond_init(reg_cond_t *c) { pthread_cond_init(c, NULL); }
static void reg_cond_destroy(reg_cond_t *c) { pthread_cond_destroy(c); }
static void reg_cond_signal(reg_cond_t *c) { pthread_cond_signal(c); }
static void reg_cond_wait(reg_cond_t *c, reg_mutex_t *m) { pthread_cond_wait(c, m); }
#endif

/* Registry policy (DC-owned; tcpme stays generic). */
#define REG_TTL_MS 90000u    /* drop a listing not refreshed within this */
#define REG_PER_IP_MAX 4     /* max distinct listings per source IP */
/* Verify is a blocking connect-back, so it runs on a worker thread off the
 * accept loop. The timeout is still kept short: an unreachable address (scanner,
 * or a real server behind NAT) ties up a worker slot for the full timeout, and
 * with a single worker that serializes other pending verifies. The accept loop
 * itself never blocks on it. */
#define REG_VERIFY_TIMEOUT_MS 500u /* callback-verify connect/handshake timeout */
#define REG_IO_TIMEOUT_MS 2000u    /* per-connection recv (bounds a slow peer) */
#define REG_VERIFY_FAIL_BACKOFF_MS 30000u /* skip re-verifying a just-failed IP */
#define REG_FAIL_TRACK 64                 /* recently-failed source IPs tracked */
#define REG_JSON_INTERVAL_MS 5000u        /* how often to rewrite servers.json */
#define REG_TICK_MS 500u                  /* accept-poll tick */
/* Bound on verify jobs waiting for the worker. A flood that outpaces the worker
 * is shed here (the announce is dropped, the server retries on its next
 * heartbeat) rather than letting the queue grow without limit. */
#define REG_VERIFY_QUEUE_MAX 64
/* Grace TTL applied to entries reloaded from servers.json on boot. The file the
 * registry writes carries no per-entry last-seen timestamp (see
 * write_servers_json), so a reloaded entry's true age is unknown. We seed it a
 * short way from expiry instead of a full TTL: a server that is still up
 * re-announces within its own ~30s heartbeat and gets the full TTL refreshed,
 * while genuinely-dead entries left over from before the restart fall off
 * quickly rather than lingering a stale 90s. Pick > the server heartbeat
 * (REG_PUB_HEARTBEAT_MS = 30s) so a live server always refreshes in time. */
#define REG_RELOAD_GRACE_MS 45000u

typedef struct {
  RegistryServer_t srv;
  uint32_t last_seen;
  uint8_t seq; /* 1..99 display number, assigned once when first listed */
  bool used;
} RegEntry_t;

/* Shared between the accept loop and the verify worker: the listing table and
 * the recently-failed-IP set. g_lock guards both, plus the JSON write. */
static RegEntry_t g_table[REGISTRY_MAX_SERVERS];
static reg_mutex_t g_lock;
/* Next "Server-NN" to hand out. Bumped per newly-listed server and wraps 99->1;
 * a number in use by a listed server is skipped (see next_free_seq), so an
 * offline server's number is never silently inherited by a different machine.
 * Guarded by g_lock.
 *
 * Multi-registry note: each registry runs its own counter, so the same server
 * can be numbered differently on two independent registries. The client dedups
 * the merged list by ip:port and keeps the first-listed registry's entry, so all
 * clients agree (they ship the same common.conf order); the number only shifts,
 * uniformly, during a first-registry outage. A deterministic identity-based name
 * (so registries agree) was considered and rejected: no registry-derivable
 * identity is at once non-forgeable, stable across a home server's dynamic IP,
 * and equal across registries (the registry only trusts what it observes -- the
 * source IP and verified port). The counter trades cross-registry agreement for
 * being non-forgeable and stable-while-listed, which is the better deal. */
static uint8_t g_next_seq = 1;
static bool g_verbose = false;
static volatile sig_atomic_t g_running = 1;

/* Recently-failed source IPs. After a verify failure we skip re-verifying that
 * IP for REG_VERIFY_FAIL_BACKOFF_MS, so a host announcing an unreachable address
 * (scanner, or a real server behind NAT) can't make every heartbeat tie up a
 * worker for the full verify timeout. Keyed on IP only, so a flooder that varies
 * the announced port is still covered; only failures are tracked, so legit
 * servers (which verify fine, even several behind one NAT'd IP) are never
 * throttled. Accessed by the accept loop (cheap pre-enqueue check) and the
 * worker (records/clears results), so guarded by g_lock. */
typedef struct {
  char ip[TCPME_ADDRSTRLEN];
  uint32_t at;
  bool used;
} FailIp_t;
static FailIp_t g_fail[REG_FAIL_TRACK];

/* Verify job queue. The accept loop enqueues a parsed announce (with the source
 * IP already filled in) and returns immediately; the worker dequeues, does the
 * blocking callback_verify, and commits to g_table on success. A bounded ring
 * buffer with a condvar for the worker to wait on. g_qlock guards the ring and
 * the running flag; g_qcond wakes the worker on a new job or on shutdown. */
static RegistryServer_t g_queue[REG_VERIFY_QUEUE_MAX];
static int g_qhead = 0; /* next slot to dequeue */
static int g_qcount = 0;
static reg_mutex_t g_qlock;
static reg_cond_t g_qcond;
static bool g_worker_run = true; /* cleared under g_qlock to stop the worker */

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

/* The g_table / g_fail helpers below operate on shared state with no internal
 * locking; every caller must hold g_lock. Keeping the lock at the call sites
 * (rather than inside each helper) keeps the critical sections coarse and
 * visible and avoids accidental recursive locking when one helper calls
 * another. */

static int count_for_ip(const char *ip) {
  int n = 0;
  for (int i = 0; i < REGISTRY_MAX_SERVERS; i++)
    if (g_table[i].used && strcmp(g_table[i].srv.ip, ip) == 0)
      n++;
  return n;
}

/* Parse the NN out of a "Server-NN" label. Returns 1..99, or 0 if the name is
 * not in that form (empty, or a legacy / foreign label). */
static uint8_t seq_from_name(const char *name) {
  if (strncmp(name, "Server-", 7) != 0 || name[7] < '0' || name[7] > '9')
    return 0;
  int v = atoi(name + 7);
  return (v >= 1 && v <= 99) ? (uint8_t)v : 0;
}

/* Pick the next free display number starting at g_next_seq, skipping any number
 * a listed server still holds, and advance g_next_seq past it (wrapping 99->1).
 * Caller holds g_lock. If all of 1..99 were somehow in use (>99 live servers --
 * not reachable under the per-IP and global listing caps) it returns g_next_seq
 * anyway, accepting a duplicate rather than looping forever. */
static uint8_t next_free_seq(void) {
  for (int tries = 0; tries < 99; tries++) {
    uint8_t cand = g_next_seq;
    g_next_seq = (uint8_t)(g_next_seq % 99 + 1);
    bool taken = false;
    for (int i = 0; i < REGISTRY_MAX_SERVERS; i++)
      if (g_table[i].used && g_table[i].seq == cand) {
        taken = true;
        break;
      }
    if (!taken)
      return cand;
  }
  return g_next_seq;
}

/* Insert or refresh, keyed on ip+port. Returns false if a per-IP cap or the
 * table capacity is hit (refreshing an existing entry always succeeds).
 *
 * The display name is the registry's to assign: a refresh keeps the entry's
 * existing "Server-NN"; a fresh insert gets the next free number, except a name
 * carried in by the servers.json reload is honoured so a server that was up
 * across a restart keeps its old number. The chosen name is written back into
 * *srv so the caller can log it. */
static bool table_upsert(RegistryServer_t *srv, uint32_t now) {
  for (int i = 0; i < REGISTRY_MAX_SERVERS; i++) {
    if (g_table[i].used && g_table[i].srv.tcp_port == srv->tcp_port &&
        strcmp(g_table[i].srv.ip, srv->ip) == 0) {
      uint8_t s = g_table[i].seq;
      g_table[i].srv = *srv; /* refresh stats; the announce carries no name */
      snprintf(g_table[i].srv.name, sizeof g_table[i].srv.name, "Server-%02u", (unsigned)s);
      g_table[i].last_seen = now;
      snprintf(srv->name, sizeof srv->name, "Server-%02u", (unsigned)s);
      return true;
    }
  }
  if (count_for_ip(srv->ip) >= REG_PER_IP_MAX)
    return false;
  for (int i = 0; i < REGISTRY_MAX_SERVERS; i++) {
    if (!g_table[i].used) {
      uint8_t s = seq_from_name(srv->name); /* nonzero only on a json reload */
      if (s == 0)
        s = next_free_seq();
      g_table[i].srv = *srv;
      snprintf(g_table[i].srv.name, sizeof g_table[i].srv.name, "Server-%02u", (unsigned)s);
      g_table[i].seq = s;
      g_table[i].last_seen = now;
      g_table[i].used = true;
      snprintf(srv->name, sizeof srv->name, "Server-%02u", (unsigned)s);
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

/* Atomic write of the given list as a JSON array (temp + rename). Takes a
 * caller-supplied snapshot rather than reading g_table directly, so the file I/O
 * happens outside g_lock (the snapshot is copied under the lock by the caller). */
static void write_servers_json(const char *path, const RegistryServer_t *list, int n) {
  char tmp[1024];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *fp = fopen(tmp, "w");
  if (!fp) {
    perror("registry: open servers.json temp");
    return;
  }
  fputs("[\n", fp);
  for (int i = 0; i < n; i++) {
    char ename[REGISTRY_NAME_MAX * 2 + 1];
    json_escape(list[i].name, ename, sizeof ename);
    /* start_time is the server's reported wall-clock boot (unix seconds, 0 =
     * unknown). Emit it raw so the website can compute uptime against its own
     * "now" rather than baking a stale value into the file. */
    fprintf(fp,
            "%s  {\"ip\":\"%s\",\"port\":%u,\"name\":\"%s\",\"players\":%u,"
            "\"max\":%u,\"password\":%s,\"in_progress\":%s,\"start_time\":%llu}",
            i == 0 ? "" : ",\n", list[i].ip, list[i].tcp_port, ename,
            list[i].player_count, list[i].max_players,
            list[i].password_protected ? "true" : "false",
            list[i].in_progress ? "true" : "false",
            (unsigned long long)list[i].start_time);
  }
  fputs("\n]\n", fp);
  if (fclose(fp) != 0) {
    perror("registry: write servers.json");
    return;
  }
  if (rename(tmp, path) != 0)
    perror("registry: rename servers.json");
}

/* --- servers.json reload on boot (#75) ---
 *
 * A registry restart otherwise starts empty: every server vanishes from the
 * public list until it next heartbeats. To bridge that gap we reload the
 * servers.json we last wrote and seed g_table from it, then let the normal TTL
 * sweep retire entries that don't re-announce.
 *
 * This runs once, before the accept loop starts, while the registry is still
 * single-threaded — so seeding g_table here needs no locking. (If a verify
 * worker thread is ever added, this must stay strictly before the worker is
 * spawned, or move under that thread's table lock.)
 *
 * The parser is a deliberately small, tolerant scanner tuned to the exact shape
 * write_servers_json emits (a JSON array of flat objects, one server each). It
 * is not a general JSON parser: it looks for "key": tokens inside each {...}
 * object and reads the value that follows. The file is one the registry itself
 * wrote, so this is enough; anything it can't make sense of (missing/truncated/
 * garbage file, a malformed object) is skipped, and a completely unparseable
 * file just yields an empty table — never a crash. */

static const char *normalize_ip(const char *ip); /* defined just below */

/* Find the value text just past "<key>": within [obj, end). Returns a pointer
 * to the first non-space value char, or NULL if the key isn't present. */
static const char *json_find_value(const char *obj, const char *end, const char *key) {
  char pat[32];
  int n = snprintf(pat, sizeof pat, "\"%s\"", key);
  if (n < 0 || (size_t)n >= sizeof pat)
    return NULL;
  size_t patlen = strlen(pat);
  for (const char *p = obj; p + patlen <= end; p++) {
    if (strncmp(p, pat, patlen) != 0)
      continue;
    p += patlen;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      p++;
    if (p >= end || *p != ':')
      continue;
    p++;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      p++;
    return (p < end) ? p : NULL;
  }
  return NULL;
}

/* Read a JSON string value at *p (which points at the opening quote) into out,
 * un-escaping \" and \\ (the only escapes write_servers_json produces). Returns
 * true on success. */
static bool json_read_string(const char *p, const char *end, char *out, size_t outsz) {
  if (p >= end || *p != '"')
    return false;
  p++;
  size_t j = 0;
  while (p < end && *p != '"') {
    char c = *p;
    if (c == '\\' && p + 1 < end) {
      p++;
      c = *p;
    }
    if (j + 1 < outsz)
      out[j++] = c;
    p++;
  }
  if (j >= outsz)
    j = outsz - 1;
  out[j] = '\0';
  return (p < end && *p == '"');
}

/* Read an unsigned integer value at *p into *out. Returns true if at least one
 * digit was read. */
static bool json_read_u64(const char *p, const char *end, uint64_t *out) {
  uint64_t v = 0;
  bool any = false;
  while (p < end && *p >= '0' && *p <= '9') {
    v = v * 10u + (uint64_t)(*p - '0');
    any = true;
    p++;
  }
  *out = v;
  return any;
}

static bool json_read_bool(const char *p, const char *end, bool *out) {
  if (p + 4 <= end && strncmp(p, "true", 4) == 0) {
    *out = true;
    return true;
  }
  if (p + 5 <= end && strncmp(p, "false", 5) == 0) {
    *out = false;
    return true;
  }
  return false;
}

/* Parse one {...} object [obj, objend) into *srv. Returns true if it carried at
 * least a usable ip and non-zero port (the minimum to list). Absent optional
 * fields default to 0/false. */
static bool parse_server_object(const char *obj, const char *objend, RegistryServer_t *srv) {
  memset(srv, 0, sizeof *srv);

  const char *v = json_find_value(obj, objend, "ip");
  if (!v || !json_read_string(v, objend, srv->ip, sizeof srv->ip) || srv->ip[0] == '\0')
    return false;

  uint64_t u = 0;
  v = json_find_value(obj, objend, "port");
  if (!v || !json_read_u64(v, objend, &u) || u == 0 || u > UINT16_MAX)
    return false;
  srv->tcp_port = (uint16_t)u;

  v = json_find_value(obj, objend, "name");
  if (v)
    json_read_string(v, objend, srv->name, sizeof srv->name);

  v = json_find_value(obj, objend, "players");
  if (v && json_read_u64(v, objend, &u))
    srv->player_count = (uint8_t)u;

  v = json_find_value(obj, objend, "max");
  if (v && json_read_u64(v, objend, &u))
    srv->max_players = (uint8_t)u;

  v = json_find_value(obj, objend, "start_time");
  if (v && json_read_u64(v, objend, &u))
    srv->start_time = u;

  v = json_find_value(obj, objend, "password");
  if (v)
    json_read_bool(v, objend, &srv->password_protected);

  v = json_find_value(obj, objend, "in_progress");
  if (v)
    json_read_bool(v, objend, &srv->in_progress);

  return true;
}

/* Reload servers.json into g_table on boot. Entries are seeded as trusted (same
 * model as a live announce: an entry is just a RegistryServer_t with a
 * last_seen), but with a short grace clock (REG_RELOAD_GRACE_MS from expiry) so
 * they expire fast unless the server re-announces. They are NOT re-verified here
 * — verification is a blocking connect-back, and a still-up server proves itself
 * on its next heartbeat (which also refreshes the full TTL); a dead one simply
 * times out of the grace window. Defensive: a missing/garbage file leaves the
 * table empty and logs via rlog. Returns the number of entries seeded. */
static int load_servers_json(const char *path, uint32_t now) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    rlog("no servers.json to reload (%s); starting empty", path);
    return 0;
  }
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return 0;
  }
  long sz = ftell(fp);
  if (sz <= 0 || sz > 4 * 1024 * 1024) { /* sane upper bound; the file we write is tiny */
    fclose(fp);
    rlog("servers.json empty or implausibly large; starting empty");
    return 0;
  }
  rewind(fp);
  char *buf = malloc((size_t)sz + 1);
  if (!buf) {
    fclose(fp);
    return 0;
  }
  size_t got = fread(buf, 1, (size_t)sz, fp);
  fclose(fp);
  buf[got] = '\0';
  const char *end = buf + got;

  /* The grace clock: pretend these were last seen long enough ago that only
   * REG_RELOAD_GRACE_MS of TTL remains. Guard the subtraction for tiny TTLs. */
  uint32_t seeded_seen =
      (REG_TTL_MS > REG_RELOAD_GRACE_MS) ? now - (REG_TTL_MS - REG_RELOAD_GRACE_MS) : now;

  int loaded = 0;
  const char *p = buf;
  while (p < end) {
    /* Object bounds: from '{' to the next '}'. registry_parse_announce strips
     * control chars but a printable '}' inside a server name would end the
     * object early here, so that one entry may fail to reload — acceptable: the
     * server re-announces on its next heartbeat and is listed normally. */
    const char *obj = memchr(p, '{', (size_t)(end - p));
    if (!obj)
      break;
    const char *objend = memchr(obj, '}', (size_t)(end - obj));
    if (!objend)
      break;
    RegistryServer_t srv;
    if (parse_server_object(obj, objend, &srv)) {
      /* Re-normalize the stored IP for symmetry with the announce path. */
      const char *nip = normalize_ip(srv.ip);
      if (nip != srv.ip)
        memmove(srv.ip, nip, strlen(nip) + 1);
      if (table_upsert(&srv, seeded_seen)) {
        loaded++;
        rlog("reloaded %s:%u \"%s\"", srv.ip, srv.tcp_port, srv.name);
      }
    }
    p = objend + 1;
  }
  free(buf);
  rlog("reloaded %d server(s) from %s", loaded, path);
  return loaded;
}

/* Strip the IPv4-mapped IPv6 prefix so a plain IPv4 server shows as "1.2.3.4"
 * rather than "::ffff:1.2.3.4" (the form a dual-stack listener reports). */
static const char *normalize_ip(const char *ip) {
  if (strncmp(ip, "::ffff:", 7) == 0 && strchr(ip + 7, '.'))
    return ip + 7;
  return ip;
}

/* True if this IP failed verify within the backoff window — skip enqueueing a
 * re-verify and reject the announce cheaply. Caller holds g_lock. */
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

/* Enqueue a verify job for the worker. Returns false (and drops the announce) if
 * the queue is full -- the server retries on its next heartbeat. Wakes the
 * worker. Holds g_qlock only briefly (a struct copy), never blocks. */
static bool verify_enqueue(const RegistryServer_t *srv) {
  bool queued = false;
  reg_mutex_lock(&g_qlock);
  if (g_qcount < REG_VERIFY_QUEUE_MAX) {
    int tail = (g_qhead + g_qcount) % REG_VERIFY_QUEUE_MAX;
    g_queue[tail] = *srv;
    g_qcount++;
    queued = true;
    reg_cond_signal(&g_qcond);
  }
  reg_mutex_unlock(&g_qlock);
  return queued;
}

/* Verify worker. Blocks on the queue condvar; for each job does the blocking
 * callback_verify and, on success, commits to g_table. The backoff check is
 * repeated here (the IP may have failed since it was enqueued) so re-verifies of
 * a known-bad host are still cheap. Exits as soon as g_worker_run is cleared --
 * any jobs still queued are abandoned (they are plain structs in a ring buffer,
 * not heap, so nothing leaks) rather than drained, keeping shutdown latency to
 * at most one in-flight verify timeout instead of the whole queue. */
static REG_THREAD_FN verify_worker(void *arg) {
  (void)arg;
  for (;;) {
    RegistryServer_t srv;
    reg_mutex_lock(&g_qlock);
    while (g_worker_run && g_qcount == 0)
      reg_cond_wait(&g_qcond, &g_qlock);
    if (!g_worker_run) { /* shutdown requested; abandon any queued jobs */
      reg_mutex_unlock(&g_qlock);
      break;
    }
    srv = g_queue[g_qhead];
    g_qhead = (g_qhead + 1) % REG_VERIFY_QUEUE_MAX;
    g_qcount--;
    reg_mutex_unlock(&g_qlock);

    uint32_t now = dc_get_ticks();
    bool backoff;
    reg_mutex_lock(&g_lock);
    backoff = verify_backing_off(srv.ip, now);
    reg_mutex_unlock(&g_lock);
    if (backoff) {
      rlog("skip verify (backoff) %s:%u", srv.ip, srv.tcp_port);
      continue;
    }

    /* The blocking part -- deliberately outside any lock so a slow peer never
     * holds up the accept loop's table reads or the JSON write. */
    bool ok = callback_verify(srv.ip, srv.tcp_port);

    now = dc_get_ticks();
    reg_mutex_lock(&g_lock);
    if (ok) {
      clear_verify_fail(srv.ip);
      if (table_upsert(&srv, now))
        rlog("listed %s:%u \"%s\" (%u/%u)", srv.ip, srv.tcp_port, srv.name,
             srv.player_count, srv.max_players);
      else
        rlog("rejected %s:%u (per-IP cap or full)", srv.ip, srv.tcp_port);
    } else {
      note_verify_fail(srv.ip, now);
      rlog("verify failed %s:%u", srv.ip, srv.tcp_port);
    }
    reg_mutex_unlock(&g_lock);
  }
  return REG_THREAD_RET;
}

static int cmp_name(const void *a, const void *b) {
  const RegistryServer_t *x = a, *y = b;
  return strcmp(x->name, y->name);
}

/* Order a snapshot by display name for a stable, numeric list order. Names are
 * zero-padded ("Server-07"), so a plain strcmp sorts them 01..99 numerically.
 * The numbers themselves are assigned once when a server is first listed (see
 * table_upsert) and persist via servers.json -- this only orders the output, it
 * does not renumber. Call on a snapshot, never on g_table.
 *
 * Operators do not name servers: it prevents abusive names with no filter list,
 * and a stable registry-assigned number means "let's go to Server-02" can't be
 * confused with a different machine that later takes a freed slot. There is no
 * "Official"/endorsed label -- the registry cannot verify one. A private source
 * IP only means "co-located with this registry" (any process on the registry
 * host qualifies), not "run by the project", and common.conf is client-shared
 * and editable so it can't be the source of truth either. An endorsed badge, if
 * ever wanted, has to be an operator-controlled allowlist matched against the
 * connect-back-verified source IP -- a registry-side decision, not anything the
 * announcer can assert. */
static void sort_servers(RegistryServer_t *list, int n) {
  if (n > 1)
    qsort(list, (size_t)n, sizeof *list, cmp_name);
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
      bool backoff;
      reg_mutex_lock(&g_lock);
      backoff = verify_backing_off(srv.ip, now);
      reg_mutex_unlock(&g_lock);
      if (srv.tcp_port == 0) {
        /* nothing to verify or list */
      } else if (backoff) {
        /* Reject cheaply without enqueueing -- a known-bad IP would just fail
         * the re-verify and tie up a worker slot. */
        rlog("skip verify (backoff) %s:%u", srv.ip, srv.tcp_port);
      } else if (!verify_enqueue(&srv)) {
        rlog("verify queue full, dropped %s:%u", srv.ip, srv.tcp_port);
      } else {
        rlog("queued verify %s:%u", srv.ip, srv.tcp_port);
      }
    }
  } else if (opcode == MSG_REG_LIST_REQUEST) {
    RegistryServer_t list[REGISTRY_MAX_SERVERS];
    reg_mutex_lock(&g_lock);
    int n = table_snapshot(list, REGISTRY_MAX_SERVERS);
    reg_mutex_unlock(&g_lock);
    sort_servers(list, n); /* names are assigned at listing time; just order them */
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

  /* Bring up the shared-state lock and the verify worker before accepting. */
  reg_mutex_init(&g_lock);
  reg_mutex_init(&g_qlock);
  reg_cond_init(&g_qcond);
  /* Seed g_table from the last-written servers.json so a restart doesn't blank
   * the public list. Done before starting the verify worker, while still
   * single-threaded, so no locking is needed. */
  if (json_path) {
    load_servers_json(json_path, dc_get_ticks());
    /* Resume numbering past the highest reloaded "Server-NN" so a fresh server
     * isn't handed a number a reloaded one still holds. A server that stayed up
     * across the restart keeps its own number via the refresh path; this only
     * affects newly-listed servers afterwards. The exact counter isn't persisted,
     * so across a full 99-wrap this is approximate -- but the per-IP and capacity
     * limits keep the live set far below 99. */
    uint8_t max_seq = 0;
    for (int i = 0; i < REGISTRY_MAX_SERVERS; i++)
      if (g_table[i].used && g_table[i].seq > max_seq)
        max_seq = g_table[i].seq;
    if (max_seq > 0)
      g_next_seq = (uint8_t)(max_seq % 99 + 1);
  }
  g_worker_run = true;
  reg_thread_t worker;
  if (reg_thread_create(&worker, verify_worker, NULL) != 0) {
    fputs("registry: failed to start verify worker\n", stderr);
    reg_cond_destroy(&g_qcond);
    reg_mutex_destroy(&g_qlock);
    reg_mutex_destroy(&g_lock);
    tcpme_free_set(set);
    tcpme_close(server);
    tcpme_quit();
    return 1;
  }

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
    /* Expire and snapshot under g_lock; the file write happens afterwards from
     * the local snapshot so I/O never holds the lock. */
    RegistryServer_t snap[REGISTRY_MAX_SERVERS];
    int n = 0;
    bool do_json = json_path && now - last_json >= REG_JSON_INTERVAL_MS;
    reg_mutex_lock(&g_lock);
    table_expire(now);
    if (do_json)
      n = table_snapshot(snap, REGISTRY_MAX_SERVERS);
    reg_mutex_unlock(&g_lock);
    if (do_json) {
      sort_servers(snap, n); /* names are assigned at listing time; just order them */
      write_servers_json(json_path, snap, n);
      last_json = now;
    }
  }

  printf("registry: shutting down\n");
  /* Stop the worker: clear the run flag, wake it, and join. The worker abandons
   * any still-queued jobs (plain structs in a ring buffer, not heap, so nothing
   * leaks) and returns after at most one in-flight verify timeout. */
  reg_mutex_lock(&g_qlock);
  g_worker_run = false;
  reg_cond_signal(&g_qcond);
  reg_mutex_unlock(&g_qlock);
  reg_thread_join(worker);

  reg_cond_destroy(&g_qcond);
  reg_mutex_destroy(&g_qlock);
  reg_mutex_destroy(&g_lock);
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
