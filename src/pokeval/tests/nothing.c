#include "00_test.h"

_MAIN_HEAD_

POKEVAL_Hand_9 ranked[] = {
    {{{DH_CARD_QUEEN, DH_SUIT_HEARTS},
      {DH_CARD_TEN, DH_SUIT_SPADES},
      {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_THREE, DH_SUIT_DIAMONDS},
      {DH_CARD_FOUR, DH_SUIT_HEARTS},
      {DH_CARD_NULL, DH_CARD_NULL},
      {DH_CARD_NULL, DH_CARD_NULL}}},
    {{{DH_CARD_ACE, DH_SUIT_SPADES},
      {DH_CARD_KING, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_FIVE, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS},
      {DH_CARD_NULL, DH_CARD_NULL},
      {DH_CARD_NULL, DH_CARD_NULL}}},
    {{{DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_KING, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS},
      {DH_CARD_NULL, DH_CARD_NULL},
      {DH_CARD_NULL, DH_CARD_NULL}}},
};

for (size_t i = 0; i < sizeof ranked / sizeof ranked[0]; i++)
  assert(POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(&ranked[i])) == POKEVAL_HIGH_CARD);

POKEVAL_NeedComparing need_comparing[3] = {
    {.id = 0, .hand = ranked[0]},
    {.id = 1, .hand = ranked[1]},
    {.id = 2, .hand = ranked[2]},
};

int n_wins = POKEVAL_compare_hands(need_comparing, 3, false);
assert(n_wins == 1);
fprintf(stderr, "winners: %d\n", n_wins);
assert(need_comparing[1].won);

POKEVAL_Hand_9 ranked2[] = {
    {{{DH_CARD_QUEEN, DH_SUIT_HEARTS},
      {DH_CARD_TEN, DH_SUIT_SPADES},
      {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_THREE, DH_SUIT_DIAMONDS},
      {DH_CARD_FOUR, DH_SUIT_HEARTS},
      {DH_CARD_NULL, DH_CARD_NULL},
      {DH_CARD_NULL, DH_CARD_NULL}}},
    {{{DH_CARD_TEN, DH_SUIT_SPADES},
      {DH_CARD_KING, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_FIVE, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS},
      {DH_CARD_NULL, DH_CARD_NULL},
      {DH_CARD_NULL, DH_CARD_NULL}}},
    {{{DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_KING, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS},
      {DH_CARD_NULL, DH_CARD_NULL},
      {DH_CARD_NULL, DH_CARD_NULL}}},
};

for (size_t i = 0; i < sizeof ranked2 / sizeof ranked2[0]; i++)
  assert(POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(&ranked2[i])) == POKEVAL_HIGH_CARD);

POKEVAL_NeedComparing need_comparing2[3] = {
    {.id = 0, .hand = ranked2[0]},
    {.id = 1, .hand = ranked2[1]},
    {.id = 2, .hand = ranked2[2]},
};

n_wins = POKEVAL_compare_hands(need_comparing2, 3, false);
assert(n_wins == 1);
fprintf(stderr, "winners: %d\n", n_wins);
assert(need_comparing2[2].won);

// memcpy(&need_comparing[0].hand, &hands[i], sizeof hands[i]);
// memcpy(&need_comparing[1].hand, &hands[i + 1], sizeof hands[i + 1]);
// memcpy(&need_comparing[2].hand, &hands[i + 2], sizeof hands[i + 2]);

_MAIN_TAIL_
