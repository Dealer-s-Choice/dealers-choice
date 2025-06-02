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
  const bool test_mode = true;                                                                     \
  char addr[] = "127.0.0.1";                                                                       \
  SocketContext_t socket_context[2];                                                               \
                                                                                                   \
  for (int i = 0; i < 2; i++) {                                                                    \
    socket_context[i] = run_client(addr, &sdl_context, &font, test_mode);                          \
    sleep(1);                                                                                      \
    assert(socket_context[i].sock != NULL);                                                        \
  }                                                                                                \
                                                                                                   \
  for (int game = 0; game < n_passes; game++) {                                                    \
    fprintf(stderr, "\n-#- game: %d\n", game);                                                     \
    const int n_seconds = 1;                                                                       \
    sleep(n_seconds);                                                                              \
    int i;                                                                                         \
                                                                                                   \
    for (int recv = 0; recv < 2; recv++) {                                                         \
      sleep(n_seconds);                                                                            \
      for (i = 0; i < 2; i++) {                                                                    \
        assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],      \
                               &client_state[i], socket_context[i].id) != RECV_ERROR);             \
        assert(socket_context[i].sock != NULL);                                                    \
      }                                                                                            \
    }                                                                                              \
                                                                                                   \
    const int dealer_id = game_state[0].dealer_id;                                                 \
    switch (game) {                                                                                \
    case 0:                                                                                        \
      assert(dealer_id == 0);                                                                      \
      break;                                                                                       \
    case 1:                                                                                        \
      assert(dealer_id == 1);                                                                      \
      break;                                                                                       \
    case 2:                                                                                        \
      assert(dealer_id == 0);                                                                      \
      break;                                                                                       \
    }
