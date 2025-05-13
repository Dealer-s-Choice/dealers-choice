#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include "game.h"
#include "net.h"
#include "server.h"

#define _MAIN_HEAD_                                                                                \
  int main(int argc, char *argv[]) {                                                               \
    (void)argc;                                                                                    \
    (void)argv;

#define _MAIN_TAIL_                                                                                \
  return 0;                                                                                        \
  }
