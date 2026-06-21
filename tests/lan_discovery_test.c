/*
 * lan_discovery_test.c — LAN discovery query/response round trip.
 *
 * Exercises the real API: a responder answers a client's query and the client
 * parses the reply. lan_discovery_query() also hits loopback, so this completes
 * deterministically without depending on limited-broadcast delivery (which is
 * not reliable to a same-host listener and may be absent in CI).
 */

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lan_discovery.h"

static bool wait_ready(tcpme_socket_t s, uint32_t ms) {
  tcpme_set_t *set = tcpme_alloc_set(1);
  assert(set != NULL);
  assert(tcpme_add_socket(set, s) == 0);
  bool ok = tcpme_check_sockets(set, ms) > 0 && tcpme_socket_ready(set, s);
  tcpme_free_set(set);
  return ok;
}

int main(void) {
  assert(tcpme_init() == 0);

  tcpme_socket_t resp = lan_discovery_open_responder(LAN_DISCOVERY_PORT);
  assert(tcpme_socket_valid(resp));
  tcpme_socket_t cli = lan_discovery_open_client();
  assert(tcpme_socket_valid(cli));

  LanGameInfo_t adv = {0};
  adv.tcp_port = 22777;
  adv.player_count = 2;
  adv.max_players = 5;
  adv.password_protected = true;
  adv.in_progress = false;
  strcpy(adv.name, "test table");

  assert(lan_discovery_query(cli, LAN_DISCOVERY_PORT));

  /* Responder receives the query (via loopback) and replies. */
  assert(wait_ready(resp, 2000));
  assert(lan_discovery_answer(resp, &adv));

  /* Client parses the reply. */
  assert(wait_ready(cli, 2000));
  LanGameInfo_t got = {0};
  assert(lan_discovery_read_response(cli, &got));

  assert(got.tcp_port == 22777);
  assert(got.player_count == 2);
  assert(got.max_players == 5);
  assert(got.password_protected == true);
  assert(got.in_progress == false);
  assert(strcmp(got.name, "test table") == 0);
  assert(strlen(got.ip) > 0);

  /* --- IPv6 link-local multicast round trip (best-effort) ---
   * Same-host multicast loopback is not available everywhere (CI runners, hosts
   * with IPv6 disabled), so this path is non-fatal: it validates the round trip
   * where the environment supports it, and is skipped otherwise. The real
   * cross-machine check is a two-host manual test. */
  {
    tcpme_socket_t resp6 = lan_discovery_open_responder6(LAN_DISCOVERY_PORT);
    tcpme_socket_t cli6 = lan_discovery_open_client6();
    if (tcpme_socket_valid(resp6) && tcpme_socket_valid(cli6) && lan_discovery_query6(cli6) &&
        wait_ready(resp6, 1000) && lan_discovery_answer6(resp6, &adv) && wait_ready(cli6, 1000)) {
      LanGameInfo_t g6 = {0};
      if (lan_discovery_read_response6(cli6, &g6)) {
        assert(g6.tcp_port == 22777);
        assert(g6.max_players == 5);
        assert(strcmp(g6.name, "test table") == 0);
        assert(strlen(g6.ip) > 0);
        printf("ipv6 discovery round trip OK (ip=%s)\n", g6.ip);
      }
    } else {
      printf("ipv6 discovery round trip skipped (no multicast loopback here)\n");
    }
    if (tcpme_socket_valid(resp6))
      tcpme_close(resp6);
    if (tcpme_socket_valid(cli6))
      tcpme_close(cli6);
  }

  /* lan_discovery_query() sends to both broadcast and loopback, and on some
   * hosts the broadcast copy is also delivered locally -- so the responder may
   * still have a duplicate query queued. Drain any pending datagrams so the
   * junk check below sees only the junk. */
  {
    char tmp[64];
    char tip[TCPME_ADDRSTRLEN];
    uint16_t tport;
    tcpme_set_t *ds = tcpme_alloc_set(1);
    assert(ds && tcpme_add_socket(ds, resp) == 0);
    while (tcpme_check_sockets(ds, 0) > 0 && tcpme_socket_ready(ds, resp))
      tcpme_udp_recvfrom(resp, tmp, sizeof(tmp), tip, sizeof(tip), &tport);
    tcpme_free_set(ds);
  }

  /* A non-DCLAN datagram to the responder must be ignored, not answered. */
  tcpme_socket_t noise = tcpme_udp_open(0, false);
  assert(tcpme_socket_valid(noise));
  const char junk[] = "hello";
  char rbuf[TCPME_ADDRPORTSTRLEN];
  assert(tcpme_get_local_addr(resp, rbuf, sizeof(rbuf)));
  /* Send junk to the responder's port on loopback. */
  {
    const char *p = strrchr(rbuf, ':');
    assert(p);
    uint16_t rport = (uint16_t)atoi(p + 1);
    assert(tcpme_udp_sendto(noise, "127.0.0.1", rport, junk, (int)sizeof(junk)) ==
           (int)sizeof(junk));
  }
  assert(wait_ready(resp, 2000));
  LanGameInfo_t dummy = {0};
  dummy.tcp_port = 1;
  /* answer() consumes the junk datagram and returns false (no reply sent). */
  assert(lan_discovery_answer(resp, &dummy) == false);

  tcpme_close(noise);
  tcpme_close(resp);
  tcpme_close(cli);
  tcpme_quit();
  return 0;
}
