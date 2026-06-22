/*
 lan_fuzz.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Initial version written by Claude (Opus 4.8, an LLM by Anthropic) at Andy's
 direction.
*/

/*
 * Fuzz the LAN-discovery wire parser with random + mutated datagrams.
 *
 * Surface: any host on the LAN (or anything that can reach the UDP port) can
 * send a discovery datagram. Two parse paths handle untrusted bytes:
 *   - lan_discovery_answer()        : the responder's query parse (valid_header)
 *   - lan_discovery_read_response() : the client's response parse (parse_response,
 *                                     which reads a name_len byte and copies that
 *                                     many bytes into a fixed LAN_NAME_MAX buffer)
 *
 * parse_response is the higher-risk one: it trusts a length byte from the wire
 * and must clamp it against both LAN_NAME_MAX and the datagram's remaining
 * bytes. We drive both parsers the real way -- send a fuzzed datagram on
 * loopback UDP, then call the parser on the receiving socket -- so the actual
 * recvfrom + parse path runs, not a reimplementation. Under ASan/UBSan any OOB
 * read/write the length handling might commit aborts the test.
 *
 * Usage: tests/test_lan_fuzz [count] [seed]
 * Env overrides: DC_FUZZ_N (count), DC_FUZZ_SEED (seed).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_util.h"
#include "lan_discovery.h"
#include "tcpme/tcpme.h"

/* Mirror lan_discovery.c's private wire constants so the harness can build
 * structurally-valid datagrams to mutate. Kept in sync by hand; if the format
 * changes, the build still compiles (these only shape the *valid* seed). */
#define F_LAN_MAGIC "DCLAN"
#define F_LAN_MAGIC_LEN 5
#define F_LAN_DISC_VERSION 1
#define F_LAN_MSG_QUERY 'Q'
#define F_LAN_MSG_RESPONSE 'R'
#define F_LAN_HDR_LEN (F_LAN_MAGIC_LEN + 2)
#define F_LAN_RESP_FIXED_LEN (F_LAN_HDR_LEN + 2 + 1 + 1 + 1 + 4 + 1)
#define F_LAN_PKT_MAX (F_LAN_RESP_FIXED_LEN + LAN_NAME_MAX)
/* Allow oversize datagrams beyond a legal packet so truncation/overflow guards
 * are tested on both sides of the boundary. */
#define F_DGRAM_MAX (F_LAN_PKT_MAX + 64)

static size_t build_valid_response(uint8_t *out) {
  const char *name = "fuzz table";
  size_t name_len = strlen(name);
  size_t o = 0;
  memcpy(out + o, F_LAN_MAGIC, F_LAN_MAGIC_LEN);
  o += F_LAN_MAGIC_LEN;
  out[o++] = F_LAN_MSG_RESPONSE;
  out[o++] = F_LAN_DISC_VERSION;
  out[o++] = 0x59; /* port hi */
  out[o++] = 0x09; /* port lo */
  out[o++] = 2;    /* player_count */
  out[o++] = 5;    /* max_players */
  out[o++] = 0x03; /* flags */
  out[o++] = 0x12; /* instance_id */
  out[o++] = 0x34;
  out[o++] = 0x56;
  out[o++] = 0x78;
  out[o++] = (uint8_t)name_len;
  memcpy(out + o, name, name_len);
  o += name_len;
  return o;
}

static size_t build_valid_query(uint8_t *out) {
  size_t o = 0;
  memcpy(out + o, F_LAN_MAGIC, F_LAN_MAGIC_LEN);
  o += F_LAN_MAGIC_LEN;
  out[o++] = F_LAN_MSG_QUERY;
  out[o++] = F_LAN_DISC_VERSION;
  return o;
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

  if (tcpme_init() != 0) {
    fputs("lan_fuzz: tcpme_init failed\n", stderr);
    return 1;
  }

  /* The "victim" socket whose parser we fuzz: it binds an ephemeral port and we
   * unicast fuzzed datagrams to it from a separate sender, then run the parser
   * on it. lan_discovery_answer / read_response both just recvfrom + parse. */
  tcpme_socket_t victim = tcpme_udp_open(0, false);
  tcpme_socket_t sender = tcpme_udp_open(0, false);
  if (!tcpme_socket_valid(victim) || !tcpme_socket_valid(sender)) {
    fputs("lan_fuzz: could not open UDP sockets\n", stderr);
    tcpme_quit();
    return 1;
  }

  char vaddr[TCPME_ADDRPORTSTRLEN];
  if (!tcpme_get_local_addr(victim, vaddr, sizeof(vaddr))) {
    fputs("lan_fuzz: could not read victim addr\n", stderr);
    tcpme_close(victim);
    tcpme_close(sender);
    tcpme_quit();
    return 1;
  }
  const char *colon = strrchr(vaddr, ':');
  uint16_t vport = colon ? (uint16_t)atoi(colon + 1) : 0;

  tcpme_set_t *set = tcpme_alloc_set(1);
  tcpme_add_socket(set, victim);

  fuzz_rng_t rng;
  fuzz_srand(&rng, seed, seed ^ 0x33cc33ccu);

  static uint8_t valid_resp[F_DGRAM_MAX];
  static uint8_t valid_query[F_DGRAM_MAX];
  static uint8_t dgram[F_DGRAM_MAX];
  size_t resp_len = build_valid_response(valid_resp);
  size_t query_len = build_valid_query(valid_query);

  LanGameInfo_t info = {0};
  info.tcp_port = 22777;
  info.player_count = 1;
  info.max_players = 5;
  strcpy(info.name, "responder");
  info.instance_id = 0xdeadbeef;

  long answered = 0, read = 0;

  for (long i = 0; i < count; i++) {
    /* Choose what to send and which parser to run on the receiving end. We test
     * read_response (parse_response) and answer (valid_header). Mix random
     * datagrams with mutations of a valid response/query. */
    bool target_read = (fuzz_bounded(&rng, 2) == 0);
    size_t len;
    uint32_t mode = fuzz_bounded(&rng, 3);
    if (mode == 0) {
      len = fuzz_bounded(&rng, F_DGRAM_MAX + 1);
      fuzz_fill(&rng, dgram, len);
    } else if (target_read) {
      len = fuzz_mutate(&rng, valid_resp, resp_len, dgram, sizeof(dgram));
    } else {
      len = fuzz_mutate(&rng, valid_query, query_len, dgram, sizeof(dgram));
    }

    if (tcpme_udp_sendto(sender, "127.0.0.1", vport, dgram, (int)len) != (int)len)
      continue; /* zero-length or send hiccup; skip */

    /* Wait for the datagram to arrive, then run the parser. */
    if (tcpme_check_sockets(set, 200) <= 0 || !tcpme_socket_ready(set, victim))
      continue;

    if (target_read) {
      LanGameInfo_t out;
      memset(&out, 0, sizeof(out));
      (void)lan_discovery_read_response(victim, &out);
      read++;
    } else {
      /* answer() may unicast a reply back to the sender on a valid query; that
       * is fine (sender just ignores it). What matters is the parse doesn't UB. */
      (void)lan_discovery_answer(victim, &info);
      answered++;
    }
  }

  /* Drain any replies answer() sent back so they don't linger (cosmetic). */
  {
    char tmp[F_DGRAM_MAX];
    char tip[TCPME_ADDRSTRLEN];
    uint16_t tport;
    tcpme_set_t *ds = tcpme_alloc_set(1);
    tcpme_add_socket(ds, sender);
    while (tcpme_check_sockets(ds, 0) > 0 && tcpme_socket_ready(ds, sender))
      tcpme_udp_recvfrom(sender, tmp, sizeof(tmp), tip, sizeof(tip), &tport);
    tcpme_free_set(ds);
  }

  tcpme_free_set(set);
  tcpme_close(victim);
  tcpme_close(sender);
  tcpme_quit();

  printf("lan_fuzz: %ld iterations (%ld read, %ld answer), seed %llu, OK\n", count, read, answered,
         (unsigned long long)seed);
  return 0;
}
