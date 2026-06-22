/*
 registry_fuzz.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Initial version written by Claude (Opus 4.8, an LLM by Anthropic) at Andy's
 direction.
*/

/*
 * Fuzz the registry wire parsers with random + mutated bytes.
 *
 * Internet-facing surface: the public registry (registry.dealers-choice-foss.dev)
 * accepts MSG_REG_ANNOUNCE from any server, and clients accept MSG_REG_LIST from
 * the registry. A hostile peer on either side feeds arbitrary bytes into:
 *   - registry_parse_announce()  (registry side; ServerAnnounce protobuf)
 *   - server_list__unpack()      (client side; the ServerList parse that
 *                                 registry_recv_list runs after deframing)
 *
 * Both are protobuf-c unpacks followed by field copies into fixed-size structs.
 * The harness asserts only that no input crashes / reads or writes out of
 * bounds / triggers UB -- it makes no claim about the *meaning* of a parse,
 * only that malformed input is handled gracefully. Run under ASan/UBSan, any
 * violation aborts the test.
 *
 * Usage: tests/test_registry_fuzz [count] [seed]
 * Env overrides: DC_FUZZ_N (count), DC_FUZZ_SEED (seed).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_util.h"
#include "registry.h"

#define MAX_BUF 70000 /* a touch over the 65536 frame cap the real code enforces */

/* Build a well-formed ServerAnnounce payload (no opcode/frame, matching what
 * registry_parse_announce expects). Returns the packed length. */
static size_t build_valid_announce(uint8_t *buf, size_t cap) {
  ServerAnnounce msg = SERVER_ANNOUNCE__INIT;
  msg.registry_version = REGISTRY_PROTOCOL_VERSION;
  msg.name = "Fuzz Table \x01\x02 with control bytes";
  msg.tcp_port = 22777;
  msg.player_count = 3;
  msg.max_players = 5;
  msg.password_protected = true;
  msg.in_progress = true;
  msg.start_time = 1750000000ull;

  size_t len = server_announce__get_packed_size(&msg);
  if (len > cap)
    return 0;
  server_announce__pack(&msg, buf);
  return len;
}

/* Build a well-formed ServerList payload (the body server_list__unpack parses,
 * i.e. the frame payload past the opcode). Returns the packed length. */
static size_t build_valid_list(uint8_t *buf, size_t cap) {
  enum { N = 4 };
  ServerEntry entries[N];
  ServerEntry *ptrs[N];
  for (int i = 0; i < N; i++) {
    server_entry__init(&entries[i]);
    entries[i].ip = "203.0.113.7";
    entries[i].tcp_port = (uint32_t)(22777 + i);
    entries[i].name = "Listed Server";
    entries[i].player_count = (uint32_t)i;
    entries[i].max_players = 8;
    entries[i].password_protected = (i & 1) != 0;
    entries[i].in_progress = (i & 2) != 0;
    entries[i].start_time = 1750000000ull + (uint64_t)i;
    ptrs[i] = &entries[i];
  }
  ServerList list = SERVER_LIST__INIT;
  list.n_servers = N;
  list.servers = ptrs;

  size_t len = server_list__get_packed_size(&list);
  if (len > cap)
    return 0;
  server_list__pack(&list, buf);
  return len;
}

/* Run registry_parse_announce on a buffer; result is intentionally discarded. */
static void hammer_announce(const uint8_t *data, size_t len) {
  RegistryServer_t out;
  /* The function memsets out on the success path but not on every early-return,
   * so pre-init to keep the (discarded) result well-defined for ASan. */
  memset(&out, 0, sizeof(out));
  (void)registry_parse_announce(data, len, &out);
}

/* Run the ServerList parse the way registry_recv_list does after deframing, plus
 * the same field copies into RegistryServer_t (the part that could index/copy
 * out of bounds on a malformed list). */
static void hammer_list(const uint8_t *data, size_t len) {
  ServerList *list = server_list__unpack(NULL, len, data);
  if (!list)
    return;
  RegistryServer_t out[REGISTRY_MAX_SERVERS];
  int n = 0;
  for (size_t i = 0; i < list->n_servers && n < REGISTRY_MAX_SERVERS; i++) {
    ServerEntry *e = list->servers[i];
    RegistryServer_t *d = &out[n++];
    memset(d, 0, sizeof(*d));
    if (e->ip)
      snprintf(d->ip, sizeof(d->ip), "%s", e->ip);
    d->tcp_port = (uint16_t)e->tcp_port;
    d->player_count = (uint8_t)e->player_count;
    d->max_players = (uint8_t)e->max_players;
    d->password_protected = e->password_protected;
    d->in_progress = e->in_progress;
    d->start_time = e->start_time;
    /* name copy mirrors sanitize_name's bound (cap + NUL) */
    if (e->name)
      snprintf(d->name, sizeof(d->name), "%s", e->name);
  }
  server_list__free_unpacked(list, NULL);
}

int main(int argc, char **argv) {
  long count = (argc > 1) ? strtol(argv[1], NULL, 10) : 20000;
  uint64_t seed = (argc > 2) ? strtoull(argv[2], NULL, 10) : 1;
  const char *env_n = getenv("DC_FUZZ_N");
  const char *env_seed = getenv("DC_FUZZ_SEED");
  if (env_n)
    count = strtol(env_n, NULL, 10);
  if (env_seed)
    seed = strtoull(env_seed, NULL, 10);
  if (count <= 0)
    count = 20000;

  fuzz_rng_t rng;
  fuzz_srand(&rng, seed, seed ^ 0xa5a5a5a5u);

  static uint8_t valid_announce[MAX_BUF];
  static uint8_t valid_list[MAX_BUF];
  static uint8_t buf[MAX_BUF];

  size_t ann_len = build_valid_announce(valid_announce, sizeof(valid_announce));
  size_t list_len = build_valid_list(valid_list, sizeof(valid_list));

  for (long i = 0; i < count; i++) {
    /* Mix: ~1/3 pure-random buffers, ~2/3 mutated valid messages (alternating
     * announce / list). Pure-random exercises the protobuf framing decoder;
     * mutations stay near the structured format to reach deeper field copies. */
    uint32_t mode = fuzz_bounded(&rng, 3);
    if (mode == 0) {
      uint32_t len = fuzz_bounded(&rng, 512); /* short buffers dominate */
      fuzz_fill(&rng, buf, len);
      if (i & 1)
        hammer_announce(buf, len);
      else
        hammer_list(buf, len);
    } else if (mode == 1) {
      size_t len = fuzz_mutate(&rng, valid_announce, ann_len, buf, sizeof(buf));
      hammer_announce(buf, len);
    } else {
      size_t len = fuzz_mutate(&rng, valid_list, list_len, buf, sizeof(buf));
      hammer_list(buf, len);
    }
  }

  printf("registry_fuzz: %ld iterations, seed %llu, OK\n", count, (unsigned long long)seed);
  return 0;
}
