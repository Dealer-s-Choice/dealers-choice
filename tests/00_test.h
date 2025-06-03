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

#define _MAIN_HEAD_                                                                                \
  int main(int argc, char *argv[]) {                                                               \
    (void)argc;                                                                                    \
    (void)argv;

#define _MAIN_TAIL_                                                                                \
  return 0;                                                                                        \
  }

// Let's only do macros like this to ease commonly used blocks in the test suite ;)...
#define _SETUP_SOCKET_CONTEXT_                                                                     \
  int n_passes = 3;                                                                                \
                                                                                                   \
  char *ptr_n_passes = getenv("DC_NPASSES");                                                       \
  if (ptr_n_passes)                                                                                \
    n_passes = atoi(ptr_n_passes);                                                                 \
  else if (argc > 1)                                                                               \
    n_passes = atoi(argv[1]);                                                                      \
                                                                                                   \
  SdlContext_t sdl_context = {0};                                                                  \
  Font_t font = {0};                                                                               \
                                                                                                   \
  GameState_t game_state[2] = {0};                                                                 \
  ClientState_t client_state[2] = {0};                                                             \
  PlayerConfig_t player_config = get_player_config();                                              \
  snprintf(player_config.host, sizeof(player_config.host), "127.0.0.1");                           \
  const bool test_mode = true;                                                                     \
  SocketContext_t socket_context[2];                                                               \
  const int n_seconds = 1;                                                                         \
                                                                                                   \
  for (int i = 0; i < 2; i++) {                                                                    \
    socket_context[i] =                                                                            \
        get_socket_context_and_run_client(&player_config, &sdl_context, &font, test_mode);         \
    assert(socket_context[i].sock != NULL);                                                        \
    sleep(n_seconds);                                                                              \
  }                                                                                                \
                                                                                                   \
  for (int game = 0; game < n_passes; game++) {                                                    \
    fprintf(stderr, "\n-#- game: %d\n", game);                                                     \
    sleep(n_seconds);                                                                              \
    int i;                                                                                         \
                                                                                                   \
    for (int recv = 0; recv < 2; recv++) {                                                         \
      for (i = 0; i < 2; i++) {                                                                    \
        sleep(n_seconds);                                                                          \
        assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],      \
                               &client_state[i], socket_context[i].id) != RECV_ERROR);             \
        assert(socket_context[i].sock != NULL);                                                    \
      }                                                                                            \
    }                                                                                              \
                                                                                                   \
    int8_t *dealer_id = &game_state[0].dealer_id;                                                  \
    const int expected_dealer_turn[3][3] = {{0, 0}, {1, 1}, {2, 0}};                               \
    assert(expected_dealer_turn[game][1] == *dealer_id);

#define _SOCKET_CLEANUP_AND_NET_QUIT_                                                              \
  sleep(2);                                                                                        \
  for (int i = 0; i < 2; i++) {                                                                    \
    socket_cleanup(socket_context[i].sock, socket_context[i].set);                                 \
  }                                                                                                \
  SDLNet_Quit();
