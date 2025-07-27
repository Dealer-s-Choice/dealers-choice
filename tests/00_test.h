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
  SDLNet_Quit();

#define _RECEIVE_GAME_STATE()                                                                      \
  SDL_Delay(n_ms);                                                                                 \
  for (i = 0; i < N_PLAYERS; i++) {                                                                \
    recv_status = recv_game_state(&socket_context[i], &game_state[i], &client_state[i],            \
                                  game_settings[i].client_id);                                     \
    assert(recv_status != RECV_ERROR);                                                             \
    if (recv_status == RECV_NOTHING)                                                               \
      fprintf(stderr, "Received nothing\n");                                                       \
    assert(socket_context[i].sock != NULL);                                                        \
  }

#define _SETUP_SOCKET_CONTEXT()                                                                    \
  GameSettings_t game_settings[N_PLAYERS] = {0};                                                   \
  GameState_t game_state[N_PLAYERS] = {0};                                                         \
  ClientState_t client_state[N_PLAYERS] = {0};                                                     \
  PlayerConfig_t player_config = get_player_config();                                              \
                                                                                                   \
  const bool test_mode = true;                                                                     \
  SocketContext_t socket_context[N_PLAYERS] = {0};                                                 \
                                                                                                   \
  Path_t path = {0};                                                                               \
                                                                                                   \
  ERecvStatus_t recv_status;                                                                       \
                                                                                                   \
  for (int i = 0; i < N_PLAYERS; i++) {                                                            \
    socket_context[i] = get_socket_context_and_run_client(&player_config, "127.0.0.1", NULL, NULL, \
                                                          &path, test_mode);                       \
    assert(socket_context[i].sock != NULL);                                                        \
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
    const int expected_dealer_turn[3][3] = {{0, 0}, {1, 1}, {2, 2}};                               \
    assert(expected_dealer_turn[game][1] == *dealer_id);
