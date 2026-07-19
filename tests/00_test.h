#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include "client.h"
#include "dc_config.h"
#include "debug.h"
#include "game.h"
#include "net.h"
#include "server.h"
#include "util.h"

#define N_PLAYERS 3
#define STARTING_N_COINS 20000

extern const uint32_t n_ms;
extern int n_passes;

/* The port a socket test binds when DC_PORT isn't in the environment. tests/
 * meson.build owns port allocation and injects each test's unique port via
 * -DDC_TEST_DEFAULT_PORT, so a by-hand binary run picks the same port CI gave
 * it (and distinct tests never collide). This literal is only the safety net
 * for a test binary built without the define. */
#ifndef DC_TEST_DEFAULT_PORT
#define DC_TEST_DEFAULT_PORT 30000
#endif

/* Resolve the test server port: the DC_PORT env override when it names a valid
 * 1..65535 port, else the compile-time DC_TEST_DEFAULT_PORT. */
static inline uint16_t dc_test_port(void) {
  const char *env = getenv("DC_PORT");
  if (env) {
    unsigned long v = strtoul(env, NULL, 10);
    if (v > 0 && v <= 65535)
      return (uint16_t)v;
  }
  return DC_TEST_DEFAULT_PORT;
}

#define _MAIN_HEAD_                                                                                \
  int main(int argc, char *argv[]) {                                                               \
                                                                                                   \
    char *ptr_n_passes = getenv("DC_NPASSES");                                                     \
    if (ptr_n_passes)                                                                              \
      n_passes = atoi(ptr_n_passes);                                                               \
    else if (argc > 1)                                                                             \
      n_passes = atoi(argv[1]);

#define _MAIN_TAIL_                                                                                \
  return 0;                                                                                        \
  }

#define _SOCKET_CLEANUP_AND_NET_QUIT_                                                              \
  SDL_Delay(2000);                                                                                 \
  for (int i = 0; i < N_PLAYERS; i++) {                                                            \
    socket_cleanup(&socket_context[i]);                                                            \
  }                                                                                                \
  tcpme_quit();                                                                                    \
  SDL_Quit();

#define _RECEIVE_GAME_STATE()                                                                      \
  SDL_Delay(n_ms);                                                                                 \
  for (i = 0; i < N_PLAYERS; i++) {                                                                \
    recv_status = recv_game_state(&socket_context[i], &game_state[i], &client_state[i],            \
                                  game_settings[i].client_id);                                     \
    assert(recv_status != RECV_ERROR);                                                             \
    if (recv_status == RECV_NOTHING)                                                               \
      fprintf(stderr, "Received nothing\n");                                                       \
    assert(tcpme_socket_valid(socket_context[i].sock));                                            \
  }

#define _SETUP_SOCKET_CONTEXT()                                                                    \
  if (SDL_Init(0) == -1) {                                                                         \
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());                                      \
    return 1;                                                                                      \
  }                                                                                                \
  if (tcpme_init() != 0) {                                                                         \
    fprintf(stderr, "tcpme_init failed: %s\n", tcpme_get_error());                                 \
    SDL_Quit();                                                                                    \
    return 1;                                                                                      \
  }                                                                                                \
  GameSettings_t game_settings[N_PLAYERS] = {0};                                                   \
  GameState_t game_state[N_PLAYERS] = {0};                                                         \
  ClientState_t client_state[N_PLAYERS] = {0};                                                     \
  PlayerConfig_t player_config = get_player_config();                                              \
  CliArgs_t cli_args = {0};                                                                        \
                                                                                                   \
  dc_test_mode = true;                                                                             \
  SocketContext_t socket_context[N_PLAYERS] = {0};                                                 \
                                                                                                   \
  Path_t path = {0};                                                                               \
                                                                                                   \
  ERecvStatus_t recv_status;                                                                       \
                                                                                                   \
  uint16_t test_port = dc_test_port();                                                            \
                                                                                                   \
  for (int i = 0; i < N_PLAYERS; i++) {                                                            \
    get_socket_context_and_run_client(&player_config, &cli_args, "127.0.0.1", test_port, NULL,     \
                                      NULL, &path, NULL, &socket_context[i]);                      \
    assert(tcpme_socket_valid(socket_context[i].sock));                                            \
    recv_game_settings(socket_context[i].sock, socket_context[i].set, &game_settings[i]);          \
    SDL_Delay(n_ms);                                                                               \
  }                                                                                                \
                                                                                                   \
  for (int game = 0; game < n_passes; game++) {                                                    \
    fprintf(stderr, "\n-#- game: %d\n", game);                                                     \
    SDL_Delay(n_ms);                                                                               \
    int i;                                                                                         \
                                                                                                   \
    _RECEIVE_GAME_STATE()                                                                          \
    _RECEIVE_GAME_STATE()                                                                          \
    _RECEIVE_GAME_STATE()                                                                          \
                                                                                                   \
    int8_t *dealer_id = &game_state[0].dealer_id;                                                  \
    const int expected_dealer_turn[] = {0, 1, 2};                                                  \
    assert(expected_dealer_turn[game] == *dealer_id);
