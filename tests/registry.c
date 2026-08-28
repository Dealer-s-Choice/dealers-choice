/*
 registry.c
 https://github.com/Dealer-s-Choice/dealers-choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Initial version written by Claude (Opus 4.8, an LLM by Anthropic) at Andy's
 direction.
*/

/* Wire round-trip tests for the server-registry announce format. Exercises the
 * protobuf pack -> registry_parse_announce path in memory (no socket), so a
 * field added to RegistryServer_t / ServerAnnounce but dropped on one side is
 * caught here. The current focus is start_time (the uptime source, added 2026).
 */

#include "00_test.h"

#include "registry.h"

#include "canfigger.h"
#include "net/lan_discovery.h"

/* Pack a ServerAnnounce the way registry_send_announce does, then parse it back
 * and confirm every field — start_time especially — survives. */
static void test_announce_roundtrip(void) {
  ServerAnnounce msg = SERVER_ANNOUNCE__INIT;
  msg.registry_version = REGISTRY_PROTOCOL_VERSION;
  msg.name = "Uptime Test";
  msg.tcp_port = 22777;
  msg.player_count = 3;
  msg.max_players = 5;
  msg.password_protected = true;
  msg.in_progress = true;
  msg.start_time = 1750000000ull; /* a fixed unix-seconds instant */

  size_t len = server_announce__get_packed_size(&msg);
  uint8_t *buf = malloc(len);
  assert(buf);
  server_announce__pack(&msg, buf);

  RegistryServer_t out = {0};
  int rc = registry_parse_announce(buf, len, &out);
  free(buf);

  assert(rc == 0);
  assert(out.tcp_port == 22777);
  assert(out.player_count == 3);
  assert(out.max_players == 5);
  assert(out.password_protected == true);
  assert(out.in_progress == true);
  assert(out.start_time == 1750000000ull);
  assert(strcmp(out.name, "Uptime Test") == 0);
}

/* An older server that never sets start_time leaves the proto3 field at its
 * default (0). The parse must surface 0 ("unknown"), not garbage — this is the
 * backward-compat contract the client's "-" uptime cell relies on. */
static void test_announce_missing_start_time(void) {
  ServerAnnounce msg = SERVER_ANNOUNCE__INIT;
  msg.registry_version = REGISTRY_PROTOCOL_VERSION;
  msg.name = "Legacy";
  msg.tcp_port = 22777;
  /* start_time deliberately left at the INIT default (0). */

  size_t len = server_announce__get_packed_size(&msg);
  uint8_t *buf = malloc(len);
  assert(buf);
  server_announce__pack(&msg, buf);

  RegistryServer_t out = {0};
  int rc = registry_parse_announce(buf, len, &out);
  free(buf);

  assert(rc == 0);
  assert(out.start_time == 0);
}

/* common.conf is not installed, so the absent-file path is what a stock install
 * actually runs: it must yield zero registries and the default discovery port
 * rather than failing. Nothing else covers it -- every other test points
 * DEALERSCHOICE_DATADIR at the source data/, which has the file. */
static void test_common_conf_absent(void) {
  char host[MAX_REGISTRIES][REGISTRY_HOST_LEN];
  uint16_t port[MAX_REGISTRIES];
  uint8_t count = 99;
  uint16_t discovery_port = 0;

  get_common_registries("dc_no_such_data_dir", host, port, &count, &discovery_port);

  assert(count == 0);
  assert(discovery_port == LAN_DISCOVERY_PORT);
}

/* The shipped template is all comments, and it also carries lan_discovery_port,
 * which is the one setting in the file that is not about registries. */
static void test_common_conf_comments_and_discovery_port(void) {
  const char *dir = "dc_test_common_conf";
  make_directory_recursive(dir);
  char *path = canfigger_path_join(dir, "common.conf");
  assert(path);
  FILE *fp = fopen(path, "w");
  assert(fp);
  fputs("# registry = registry.example.org\n"
        "lan_discovery_port = 22799\n",
        fp);
  fclose(fp);

  char host[MAX_REGISTRIES][REGISTRY_HOST_LEN];
  uint16_t port[MAX_REGISTRIES];
  uint8_t count = 99;
  uint16_t discovery_port = 0;

  get_common_registries(dir, host, port, &count, &discovery_port);

  assert(count == 0);            /* the only registry line is commented out */
  assert(discovery_port == 22799); /* ...but the discovery port still applies */

  remove(path);
  free(path);
  remove(dir);
}

_MAIN_HEAD_

test_announce_roundtrip();
test_announce_missing_start_time();
test_common_conf_absent();
test_common_conf_comments_and_discovery_port();

_MAIN_TAIL_
