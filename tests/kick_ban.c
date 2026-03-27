/*
 * kick_ban.c — integration test for admin kick and ban functionality.
 *
 * In test mode, the server grants admin privileges to ALL connected clients.
 * This test verifies:
 *
 *   1. KICK  — MSG_KICK_PLAYER from the admin disconnects the target.
 *   2. BAN   — MSG_BAN_PLAYER from the admin disconnects the target and adds
 *              their IP to the ban list.
 *   3. RECONNECT — a new connection from a banned IP is rejected by the server.
 *
 * Test layout (3 players, 5-card showdown, one game pass):
 *
 *   - Dealer = 0, first turn = 1.
 *   - Admin (player 0) kicks player 2  →  player 2 RECV_ERROR.
 *   - Admin (player 0) bans  player 1  →  player 1 RECV_ERROR.
 *     (Player 1 is the turn player; admin's message is dispatched via
 *      handle_disconnections, which runs for any non-turn ready socket.)
 *   - A raw TCP probe from 127.0.0.1 is accepted then immediately closed by
 *     the server  →  probe recv returns ≤ 0.
 */

#include "00_test.h"

/*
 * Drain all pending messages for `n` clients.  Waits n_ms first so the server
 * has time to send everything, then reads each client until RECV_NOTHING.
 * Asserts that no client returns RECV_ERROR.
 */
static void drain_all(SocketContext_t *sc, GameState_t *gs, ClientState_t *cs,
                      GameSettings_t *gset, int n) {
  SDL_Delay(n_ms * 2);
  for (int i = 0; i < n; i++) {
    ERecvStatus_t s;
    do {
      s = recv_game_state(&sc[i], &gs[i], &cs[i], gset[i].client_id);
      if (s == RECV_ERROR) {
        fprintf(stderr, "[kick_ban] Unexpected RECV_ERROR for player %d\n", i);
        assert(false);
      }
    } while (s == RECV_SUCCESS);
  }
}

int main(void) {
  if (SDL_Init(0) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "[kick_ban] SDL/SDLNet init failed: %s\n", SDL_GetError());
    return 1;
  }

  GameSettings_t game_settings[N_PLAYERS] = {0};
  GameState_t game_state[N_PLAYERS] = {0};
  ClientState_t client_state[N_PLAYERS] = {0};
  PlayerConfig_t player_config = get_player_config();
  CliArgs_t cli_args = {0};
  SocketContext_t socket_context[N_PLAYERS] = {0};
  Path_t path = {0};
  Link_t *links = NULL;

  uint16_t test_port = 22777;
  {
    const char *p = getenv("DC_PORT");
    if (p) {
      unsigned long v = strtoul(p, NULL, 10);
      if (v > 0 && v <= 65535)
        test_port = (uint16_t)v;
    }
  }

  /* Connect N_PLAYERS (3) clients.  In test mode, all clients receive admin. */
  for (int i = 0; i < N_PLAYERS; i++) {
    get_socket_context_and_run_client(&player_config, &cli_args, "127.0.0.1",
                                     test_port, NULL, NULL, &path,
                                     /*test_mode=*/true, links,
                                     &socket_context[i]);
    assert(socket_context[i].sock != NULL);
    recv_game_settings(socket_context[i].sock, socket_context[i].set,
                       &game_settings[i]);
    SDL_Delay(n_ms);
  }

  /* Drain initial game states for all players. */
  drain_all(socket_context, game_state, client_state, game_settings, N_PLAYERS);

  int8_t dealer_id = game_state[0].dealer_id;
  fprintf(stderr, "[kick_ban] dealer_id=%d\n", dealer_id);
  assert(dealer_id == 0); /* first game → slot 0 is dealer */

  /* Dealer (player 0) selects 5-card showdown. */
  assert(send_game_select(socket_context[dealer_id].sock,
                          game_choices[FIVE_CARD_SHOWDOWN].game_type,
                          false) == 0);

  /* Drain all post-deal messages (broadcast_game_state, MSG_GAME_SELECT,
   * MSG_TURN_ID, broadcast_game_state for first turn) before the kick test. */
  drain_all(socket_context, game_state, client_state, game_settings, N_PLAYERS);

  int8_t turn_id = client_state[0].turn_id;
  fprintf(stderr, "[kick_ban] turn_id=%d\n", turn_id);
  assert(turn_id == 1); /* dealer=0 → first turn=1 */

  /* ─── KICK TEST ─────────────────────────────────────────────────────────
   * Admin (player 0, non-turn) kicks player 2 (also non-turn).
   * Server processes MSG_KICK_PLAYER via handle_disconnections.           */
  fprintf(stderr, "[kick_ban] kicking player 2...\n");
  assert(send_kick_player(socket_context[0].sock, 2) == 0);
  /* Drain player 2 until RECV_ERROR (connection closed after kick).
   * We consume any messages that arrived before the kick closed the socket,
   * retrying on RECV_NOTHING to give the server time to process the kick. */
  {
    int kicked = 0;
    for (int try = 0; try < 20 && !kicked; try++) {
      ERecvStatus_t s = recv_game_state(&socket_context[2], &game_state[2],
                                        &client_state[2], game_settings[2].client_id);
      if (s == RECV_ERROR)
        kicked = 1;
      else if (s == RECV_NOTHING)
        SDL_Delay(n_ms);
      /* RECV_SUCCESS: consumed a pending message — try again */
    }
    assert(kicked);
  }
  fprintf(stderr, "[kick_ban] PASS: player 2 kicked (RECV_ERROR)\n");

  /* Drain updated game state for surviving players and verify disconnect. */
  drain_all(socket_context, game_state, client_state, game_settings, 2);
  assert(!game_state[0].player[2].is_connected);
  fprintf(stderr, "[kick_ban] PASS: player 2 disconnected in game state\n");

  /* ─── BAN TEST ──────────────────────────────────────────────────────────
   * Player 1 is still the turn player (they haven't acted yet).
   * Admin (player 0) sends MSG_BAN_PLAYER for player 1.  Because player 0's
   * socket becomes ready while it is not the turn player, the server reads
   * the message via handle_disconnections and executes ban_player.        */
  fprintf(stderr, "[kick_ban] banning player 1...\n");
  assert(send_ban_player(socket_context[0].sock, 1) == 0);
  /* Drain player 1 until RECV_ERROR (same robust approach as kick above). */
  {
    int banned = 0;
    for (int try = 0; try < 20 && !banned; try++) {
      ERecvStatus_t s = recv_game_state(&socket_context[1], &game_state[1],
                                        &client_state[1], game_settings[1].client_id);
      if (s == RECV_ERROR)
        banned = 1;
      else if (s == RECV_NOTHING)
        SDL_Delay(n_ms);
    }
    assert(banned);
  }
  fprintf(stderr, "[kick_ban] PASS: player 1 banned (RECV_ERROR)\n");

  /* Drain any pending state for the sole survivor (player 0). */
  drain_all(socket_context, game_state, client_state, game_settings, 1);
  assert(!game_state[0].player[1].is_connected);
  fprintf(stderr, "[kick_ban] PASS: player 1 disconnected in game state\n");

  /* ─── BAN RECONNECT TEST ────────────────────────────────────────────────
   * Player 1's IP (127.0.0.1) is now in the server's ban list.
   * Open a raw TCP socket to the server; the server should accept the TCP
   * handshake and then immediately close the connection.                  */
  fprintf(stderr, "[kick_ban] testing reconnect from banned IP...\n");
  IPaddress probe_addr;
  SDLNet_ResolveHost(&probe_addr, "127.0.0.1", test_port);
  TCPsocket probe_sock = SDLNet_TCP_Open(&probe_addr);
  if (probe_sock) {
    SDLNet_SocketSet probe_set = SDLNet_AllocSocketSet(1);
    SDLNet_TCP_AddSocket(probe_set, probe_sock);
    /* Wait up to 500 ms for the server to close the connection. */
    int ready = SDLNet_CheckSockets(probe_set, 500);
    assert(ready > 0 && SDLNet_SocketReady(probe_sock));
    char buf[1];
    int r = SDLNet_TCP_Recv(probe_sock, buf, sizeof(buf));
    assert(r <= 0); /* FIN or RST → banned IP was rejected */
    SDLNet_FreeSocketSet(probe_set);
    SDLNet_TCP_Close(probe_sock);
    fprintf(stderr,
            "[kick_ban] PASS: reconnect from banned IP rejected (recv=%d)\n", r);
  } else {
    /* Connection refused entirely — also a valid rejection. */
    fprintf(stderr,
            "[kick_ban] PASS: reconnect from banned IP refused outright\n");
  }

  /* Clean up. */
  SDL_Delay(1000);
  for (int i = 0; i < N_PLAYERS; i++)
    socket_cleanup(&socket_context[i]);
  SDLNet_Quit();
  SDL_Quit();

  return 0;
}
