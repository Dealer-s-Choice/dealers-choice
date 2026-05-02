#include "00_test.h"

_MAIN_HEAD_

POKEVAL_Hand_9 ranked[] = {
    {{{DH_CARD_QUEEN, DH_SUIT_HEARTS},
      {DH_CARD_TEN, DH_SUIT_SPADES},
      {DH_CARD_NINE, DH_SUIT_CLUBS},
      {DH_CARD_KING, DH_SUIT_DIAMONDS},
      {DH_CARD_JACK, DH_SUIT_HEARTS}}},
    {{{DH_CARD_TEN, DH_SUIT_SPADES},
      {DH_CARD_QUEEN, DH_SUIT_CLUBS},
      {DH_CARD_JACK, DH_SUIT_CLUBS},
      {DH_CARD_KING, DH_SUIT_DIAMONDS},
      {DH_CARD_ACE, DH_SUIT_CLUBS}}},
    {{{DH_CARD_FIVE, DH_SUIT_SPADES},
      {DH_CARD_TWO, DH_SUIT_CLUBS},
      {DH_CARD_THREE, DH_SUIT_CLUBS},
      {DH_CARD_FOUR, DH_SUIT_DIAMONDS},
      {DH_CARD_ACE, DH_SUIT_CLUBS}}},
};

for (size_t i = 0; i < sizeof ranked / sizeof ranked[0]; i++) {
  int rank = POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(&ranked[i]));
  fprintf(stderr, "%zd:%s\n", i, POKEVAL_rank[rank]);
  assert(rank == POKEVAL_STRAIGHT);
}

POKEVAL_NeedComparing need_comparing[3] = {
    {.id = 0, .hand = ranked[0]},
    {.id = 1, .hand = ranked[1]},
    {.id = 2, .hand = ranked[2]},
};

int n_wins = POKEVAL_compare_hands(need_comparing, 3, false);
assert(n_wins == 1);
fprintf(stderr, "winners: %d\n", n_wins);
assert(!need_comparing[0].won);
assert(need_comparing[1].won);
assert(!need_comparing[2].won);

/* Wheel flush (A-2-3-4-5 same suit) is a straight flush, not a royal flush */
{
  static POKEVAL_Hand_5 wheel_flush;
  int wf_faces[] = {DH_CARD_ACE, DH_CARD_TWO, DH_CARD_THREE, DH_CARD_FOUR, DH_CARD_FIVE};
  int wf_suits[] = {DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS};
  set_hand(&wheel_flush, wf_faces, wf_suits);
  short wf_rank = POKEVAL_evaluate_hand(wheel_flush);
  fprintf(stderr, "wheel flush: %s\n", POKEVAL_rank[wf_rank]);
  assert(wf_rank == POKEVAL_STRAIGHT_FLUSH);
}

_MAIN_TAIL_
