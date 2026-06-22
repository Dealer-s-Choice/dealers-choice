/*
 net_fuzz.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Initial version written by Claude (Opus 4.8, an LLM by Anthropic) at Andy's
 direction.
*/

/*
 * Fuzz the game-protocol wire parser with random + mutated bytes.
 *
 * Internet-facing surface: the live server and the GUI client exchange
 * length-prefixed, opcode-tagged frames ([size:4 BE][opcode:2 BE][protobuf]).
 * recv_game_state() is the client's deframe + opcode-dispatch + per-opcode
 * protobuf-unpack path -- the function that turns hostile bytes into a parsed
 * message. We drive it the real way: over a loopback TCP pair, writing
 * fuzzed frames the peer must parse without crashing.
 *
 * Two layers are exercised:
 *   1. recv_game_state() end-to-end over a socket -- frame size validation,
 *      the opcode switch, and the protobuf unpacks each opcode runs. To avoid
 *      blocking recv_all_tcp(), every frame is self-consistent: the 4-byte
 *      size prefix equals the number of payload bytes actually written. (Frame
 *      rejects like size==0 / size>65536 return before the payload read, so
 *      they don't block; those are covered too.)
 *   2. The standalone protobuf deserializers (deserialize_game_state /
 *      _settings / _hand / _player and get_game_select_payload) called directly
 *      on fuzzed buffers -- the same unpacks, reached without a socket.
 *
 * The GameProtocolHeader_t (DCPROTO magic + version + flags) handshake parser
 * lives in server.c's static recv_and_validate_protocol_header (out of scope
 * to edit here); its check is a fixed-size memcmp + version compare with no
 * variable-length parse, so the framing/protobuf fuzz above is where the parse
 * risk concentrates.
 *
 * Asserts only: no crash / OOB / UB on any input. Run under ASan/UBSan.
 *
 * Usage: tests/test_net_fuzz [count] [seed]
 * Env overrides: DC_FUZZ_N (count), DC_FUZZ_SEED (seed).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dc_time.h" /* dc_get_ticks / dc_sleep_ms */
#include "fuzz_util.h"
#include "game.h" /* GameSelectPayload_t, get_game_select_payload */
#include "net.h"
#include "tcpme/tcpme.h"
#include "util.h" /* dc_log_set_file */

#define MAX_PAYLOAD 4096

/* All opcodes recv_game_state switches on, so the fuzzer reaches every case
 * (including the protobuf-unpacking ones) rather than only the default. */
static const uint16_t OPCODES[] = {
    MSG_TURN_ID,           MSG_BET_CHECK_FOLD, MSG_CALL_RAISE_FOLD,  MSG_CALL_COMPLETE_FOLD,
    MSG_COMPLETE_CHECK_FOLD, MSG_DRAW_PROMPT,  MSG_PING_REQUEST,     MSG_PING_BROADCAST,
    MSG_STATUS_MESSAGE,    MSG_ACTION_ANNOUNCE, MSG_NEW_HAND,        MSG_GAME_SELECT,
    0x0000,                0xFFFF, /* unknown opcodes -> default GameState path */
};
#define N_OPCODES (sizeof(OPCODES) / sizeof(OPCODES[0]))

/* A connected TCP pair on loopback: client_out is what the harness writes
 * fuzzed frames to; ctx wraps the server-side accepted socket recv_game_state
 * reads from. Returns false on setup failure. */
static bool make_pair(tcpme_socket_t *client_out, SocketContext_t *ctx) {
  tcpme_socket_t listener = tcpme_listen("127.0.0.1", 0);
  if (!tcpme_socket_valid(listener))
    return false;

  char addr[TCPME_ADDRPORTSTRLEN];
  if (!tcpme_get_local_addr(listener, addr, sizeof(addr))) {
    tcpme_close(listener);
    return false;
  }
  const char *colon = strrchr(addr, ':');
  if (!colon) {
    tcpme_close(listener);
    return false;
  }
  uint16_t port = (uint16_t)atoi(colon + 1);

  tcpme_socket_t cli = tcpme_connect("127.0.0.1", port);
  if (!tcpme_socket_valid(cli)) {
    tcpme_close(listener);
    return false;
  }
  /* tcpme_accept polls with a zero-timeout select, so on platforms where the
   * just-connected client isn't in the listener's accept queue the instant
   * connect() returns (OpenBSD, notably) a single poll finds nothing and
   * make_pair would wrongly fail ("could not set up loopback pair"). The real
   * server never hits this because its accept loop polls repeatedly; mirror that
   * with a brief retry. */
  tcpme_socket_t srv = tcpme_accept(listener);
  for (int tries = 0; tries < 1000 && !tcpme_socket_valid(srv); tries++) {
    dc_sleep_ms(1);
    srv = tcpme_accept(listener);
  }
  tcpme_close(listener);
  if (!tcpme_socket_valid(srv)) {
    tcpme_close(cli);
    return false;
  }

  /* Bound blocking recvs so a single frame can never hang the run. recv_all_tcp
   * loops tcpme_recv until it has the requested bytes; if a fuzzed/desynced size
   * asks for more than will ever arrive, that recv blocks forever with no socket
   * timeout -- which is why the per-iteration wall-cap never fired and the run
   * timed out on Windows (the drain occasionally misses a straggler there,
   * desyncing the next size read). A short SO_RCVTIMEO turns that into a quick
   * RECV_ERROR -> drain -> resync (loopback delivery is sub-ms, so this never
   * trips on a legit frame). The real server uses SOCKET_IO_TIMEOUT_MS; the test
   * wants a much shorter value so recovery is fast. */
  tcpme_set_timeout(srv, 200);

  tcpme_set_t *set = tcpme_alloc_set(1);
  if (!set) {
    tcpme_close(cli);
    tcpme_close(srv);
    return false;
  }
  tcpme_add_socket(set, srv);

  *client_out = cli;
  ctx->sock = srv;
  ctx->set = set;
  return true;
}

static void close_pair(tcpme_socket_t client, SocketContext_t *ctx) {
  tcpme_free_set(ctx->set);
  tcpme_close(ctx->sock);
  tcpme_close(client);
}

/* Resync the stream after a rejected frame without tearing down the connection.
 *
 * A size-rejected frame leaves its body unread on the server socket (see
 * feed_frame), desyncing the stream. The original code rebuilt the whole
 * loopback pair on every rejection, but at 20k iterations that opens thousands
 * of short-lived TCP connections. Linux recycles ephemeral ports fast enough to
 * survive it; platforms with a smaller ephemeral range and slower TIME_WAIT
 * recycling do not -- macOS and the BSDs exhaust local ports (connect() fails,
 * "reconnect failed") and Windows spends so long in the connect storm the test
 * times out. Draining the leftover bytes instead keeps a single connection for
 * the whole run: equivalent resync, no port churn. */
static void drain_socket(SocketContext_t *ctx) {
  uint8_t scratch[2048];
  /* Read whatever is waiting, with NO blocking. When a size is rejected the body
   * was sent in the same burst that delivered the size feed_frame already polled,
   * so it is already buffered and a zero-timeout poll drains it. When there is
   * nothing to drain (a zero-body frame, or an unpack failure that consumed the
   * whole body) the poll finds nothing and returns at once -- never wait, or a
   * frame with nothing leftover would stall (a per-rejection wait blew up the
   * run's wall time). A body byte not yet arrived is left behind, but that just
   * desyncs the next frame into another rejection + drain, so it self-corrects. */
  tcpme_check_sockets(ctx->set, 0);
  while (tcpme_socket_ready(ctx->set, ctx->sock)) {
    int n = tcpme_recv(ctx->sock, scratch, (int)sizeof scratch);
    if (n <= 0)
      return; /* peer closed/error; a later write failure rebuilds the pair */
    tcpme_check_sockets(ctx->set, 0);
  }
}

/* Build a [opcode:2 BE][body] payload into buf; returns total length. */
static size_t build_frame_payload(fuzz_rng_t *r, uint8_t *buf, size_t cap) {
  uint16_t opcode = OPCODES[fuzz_bounded(r, N_OPCODES)];
  tcpme_put_be16(buf, opcode);
  size_t body = fuzz_bounded(r, (uint32_t)(cap - OPCODE_SIZE));
  fuzz_fill(r, buf + OPCODE_SIZE, body);
  return OPCODE_SIZE + body;
}

/* Send one self-consistent frame ([size:4 BE = payload_len][payload]) and let
 * recv_game_state parse it. The returned status lets the caller mirror the real
 * protocol: on RECV_ERROR the live server/client tears the connection down
 * rather than reading more, because a rejected frame leaves the rest of the
 * payload unread (recv_game_state returns after the 4-byte size on a size
 * rejection, without consuming the body). Reusing the socket past that point
 * would read leftover body bytes as the next frame's size and desync the
 * stream -- so the caller reconnects on RECV_ERROR. Sets *st to the parse
 * result; returns false only if the write failed (peer gone). */
static bool feed_frame(tcpme_socket_t client, SocketContext_t *ctx, GameState_t *gs,
                       ClientState_t *cs, const uint8_t *payload, size_t payload_len,
                       ERecvStatus_t *st) {
  uint8_t size_be[4];
  tcpme_put_be32(size_be, (uint32_t)payload_len);
  if (send_all_tcp(client, size_be, sizeof(size_be)) != 0)
    return false;
  if (payload_len > 0 && send_all_tcp(client, payload, payload_len) != 0)
    return false;

  /* Wait (bounded) for the just-sent frame to reach the server socket, then let
   * recv_game_state process it exactly once. Three normal outcomes:
   *   - a parsed frame (success) or a rejected one (RECV_ERROR);
   *   - RECV_NOTHING *after* delivery, which means the opcode was a ping:
   *     recv_game_state consumes pings transparently and then finds nothing
   *     after, so RECV_NOTHING here is "frame handled", not "keep waiting".
   * The earlier loop kept polling until a non-NOTHING status, so a fuzzed ping
   * burned the full per-poll timeout x50 -- seconds per such frame, which is
   * what timed the run out on slow CI. Process once on readiness instead. */
  *st = RECV_NOTHING;
  for (int spin = 0; spin < 10; spin++) {
    tcpme_check_sockets(ctx->set, 2); /* returns immediately once readable */
    if (tcpme_socket_ready(ctx->set, ctx->sock)) {
      *st = recv_game_state(ctx, gs, cs, 0);
      break;
    }
  }
  return true;
}

/* Directly fuzz the standalone protobuf deserializers (no socket). These are
 * the same unpacks the opcode switch runs, reached without framing. */
static void hammer_deserializers(const uint8_t *data, size_t len) {
  GameState_t gs;
  memset(&gs, 0, sizeof(gs));
  (void)deserialize_game_state(data, (uint32_t)len, &gs);

  (void)deserialize_game_settings(data, len);
  (void)deserialize_hand(data, len);
  (void)deserialize_player(data, len);

  /* get_game_select_payload takes the framed buffer including the opcode and a
   * size; feed it the raw bytes so its size/bounds check is exercised. It needs
   * a non-const buffer. */
  if (len <= MAX_PAYLOAD) {
    uint8_t tmp[MAX_PAYLOAD];
    memcpy(tmp, data, len);
    GameSelectPayload_t sel;
    memset(&sel, 0, sizeof(sel));
    (void)get_game_select_payload(tmp, (uint32_t)len, 0, &sel);
  }
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

  /* DC_FUZZ_MS: optional wall-clock cap in ms (0 = none). Bounds the run on slow
   * CI runners (Windows, the QEMU BSD VMs) so the socket loop can't trip the
   * meson per-test timeout no matter the per-iteration speed; it stops early
   * having fuzzed as many frames as fit. Manual count-based runs ignore it
   * unless it is set. */
  uint32_t budget_ms = 0;
  const char *env_ms = getenv("DC_FUZZ_MS");
  if (env_ms)
    budget_ms = (uint32_t)strtoul(env_ms, NULL, 10);

  if (tcpme_init() != 0) {
    fputs("net_fuzz: tcpme_init failed\n", stderr);
    return 1;
  }

  /* Malformed input makes the parser log a DC_LOG_ERROR per rejected frame --
   * expected and voluminous at fuzz scale. Route those to a throwaway file so
   * the test output (and any real ASan/UBSan report on stderr) stays readable.
   * "DC_FUZZ_LOG=1" keeps them on stderr for debugging a failure. */
  if (!getenv("DC_FUZZ_LOG")) {
#ifdef _WIN32
    dc_log_set_file("NUL");
#else
    dc_log_set_file("/dev/null");
#endif
  }

  fuzz_rng_t frng;
  fuzz_srand(&frng, seed, seed ^ 0x5a5a5a5au);

  /* A pair is reused across iterations for speed, but rebuilt whenever a frame
   * is rejected (RECV_ERROR), which desyncs the stream -- exactly what the real
   * client/server do on a framing error. A cleanly-parsed frame keeps the pair. */
  tcpme_socket_t client;
  SocketContext_t ctx;
  if (!make_pair(&client, &ctx)) {
    fputs("net_fuzz: could not set up loopback pair\n", stderr);
    tcpme_quit();
    return 1;
  }

  GameState_t gs;
  ClientState_t cs;
  memset(&gs, 0, sizeof(gs));
  memset(&cs, 0, sizeof(cs));

  static uint8_t payload[MAX_PAYLOAD];
  long framed = 0, direct = 0;
  uint32_t t_start = dc_get_ticks();

  for (long i = 0; i < count; i++) {
    if (budget_ms && dc_get_ticks() - t_start >= budget_ms)
      break; /* wall-clock cap hit (slow runner); stop cleanly */
    uint32_t mode = fuzz_bounded(&frng, 4);
    if (mode == 0) {
      /* Direct deserializer fuzz: pure-random buffer (no socket). */
      uint32_t len = fuzz_bounded(&frng, 512);
      fuzz_fill(&frng, payload, len);
      hammer_deserializers(payload, len);
      direct++;
      continue;
    }

    size_t len;
    if (mode == 1) {
      /* Framed: opcode + random body. */
      len = build_frame_payload(&frng, payload, sizeof(payload));
    } else if (mode == 2) {
      /* Framed: a degenerate size (0/1/2 bytes) to exercise the frame-size
       * lower-bound validation -- the path that over-read the buffer before the
       * size < OPCODE_SIZE guard was added. */
      uint32_t n = fuzz_bounded(&frng, 3);
      fuzz_fill(&frng, payload, n);
      len = n;
    } else {
      /* Framed: a fully random buffer of varied length as the whole payload
       * (random opcode bytes -> hits the default GameState path a lot). */
      uint32_t n = fuzz_bounded(&frng, (uint32_t)sizeof(payload));
      fuzz_fill(&frng, payload, n);
      len = n;
    }

    ERecvStatus_t st = RECV_NOTHING;
    if (!feed_frame(client, &ctx, &gs, &cs, payload, len, &st)) {
      /* Write failed: peer gone. Rebuild and retry the run. */
      close_pair(client, &ctx);
      if (!make_pair(&client, &ctx))
        break;
      continue;
    }
    framed++;

    if (st == RECV_ERROR) {
      /* A rejected frame may leave its body unread, desyncing the stream. Drain
       * it rather than rebuilding the pair -- see drain_socket on why the old
       * reconnect-per-rejection churn broke macOS/BSD/Windows CI. */
      drain_socket(&ctx);
    }
  }

  close_pair(client, &ctx);
  tcpme_quit();

  printf("net_fuzz: %ld iterations (%ld framed, %ld direct), seed %llu, OK\n", framed + direct, framed,
         direct,
         (unsigned long long)seed);
  return 0;
}
