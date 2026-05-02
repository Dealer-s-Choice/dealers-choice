#include <deckhandler.h>
#include <pokeval.h>
#include <stdio.h>
#include <string.h>

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>

#define PAD_NULL_CARDS                                                                             \
  {DH_CARD_NULL, DH_CARD_NULL}, { DH_CARD_NULL, DH_CARD_NULL }

#define _MAIN_HEAD_                                                                                \
  int main(int argc, char *argv[]) {                                                               \
    (void)argc;                                                                                    \
    (void)argv;

#define _MAIN_TAIL_                                                                                \
  return 0;                                                                                        \
  }

void set_hand(POKEVAL_Hand_5 *hand, int faces[POKEVAL_HAND_SIZE], int suits[POKEVAL_HAND_SIZE]);
