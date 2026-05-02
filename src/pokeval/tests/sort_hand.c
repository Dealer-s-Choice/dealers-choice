#include "00_test.h"

_MAIN_HEAD_

POKEVAL_Hand_5 hand = {.card = {
                           {DH_CARD_TEN, DH_SUIT_HEARTS},
                           {DH_CARD_THREE, DH_SUIT_CLUBS},
                           {DH_CARD_ACE, DH_SUIT_DIAMONDS},
                           {DH_CARD_FIVE, DH_SUIT_SPADES},
                           {DH_CARD_KING, DH_SUIT_CLUBS},
                       }};

POKEVAL_sort_hand(&hand);

int expected[] = {POKEVAL_ACE, DH_CARD_KING, DH_CARD_TEN, DH_CARD_FIVE, DH_CARD_THREE};
for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
  fprintf(stderr, "card: %d | ", hand.card[i].face_val);
  assert(hand.card[i].face_val == expected[i]);
}
putchar('\n');

_MAIN_TAIL_
