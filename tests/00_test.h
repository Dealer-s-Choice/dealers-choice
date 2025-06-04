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

#define _SOCKET_CLEANUP_AND_NET_QUIT_                                                              \
  sleep(2);                                                                                        \
  for (int i = 0; i < 2; i++) {                                                                    \
    socket_cleanup(socket_context[i].sock, socket_context[i].set);                                 \
  }                                                                                                \
  SDLNet_Quit();
