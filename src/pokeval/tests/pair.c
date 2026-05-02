#include "00_test.h"

_MAIN_HEAD_

POKEVAL_Hand_9 hands[] = {
    // Kings vs Queens
    {{{DH_CARD_QUEEN, DH_SUIT_HEARTS},
      {DH_CARD_TEN, DH_SUIT_SPADES},
      {DH_CARD_QUEEN, DH_SUIT_CLUBS},
      {DH_CARD_THREE, DH_SUIT_DIAMONDS},
      {DH_CARD_FOUR, DH_SUIT_HEARTS}}},
    {{{DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_KING, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_KING, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS}}},
    {{{DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_KING, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS}}},

    // Aces vs Twos
    {{{DH_CARD_ACE, DH_SUIT_HEARTS},
      {DH_CARD_TEN, DH_SUIT_SPADES},
      {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_THREE, DH_SUIT_DIAMONDS},
      {DH_CARD_FOUR, DH_SUIT_HEARTS}}},
    {{{DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_THREE, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_KING, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS}}},
    {{{DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_TWO, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS}}},

    // Twos vs Nothing
    {{{DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_KING, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_ACE, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS}}},
    {{{DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_KING, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS}}},
    {{{DH_CARD_TWO, DH_SUIT_HEARTS},
      {DH_CARD_TEN, DH_SUIT_SPADES},
      {DH_CARD_TWO, DH_SUIT_CLUBS},
      {DH_CARD_THREE, DH_SUIT_DIAMONDS},
      {DH_CARD_FOUR, DH_SUIT_HEARTS}}},
    // Twos tied
    {{{DH_CARD_TWO, DH_SUIT_DIAMONDS},
      {DH_CARD_TEN, DH_SUIT_CLUBS},
      {DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_THREE, DH_SUIT_CLUBS},
      {DH_CARD_FOUR, DH_SUIT_CLUBS}}},
    {{{DH_CARD_TWO, DH_SUIT_SPADES},
      {DH_CARD_ACE, DH_SUIT_CLUBS},
      {DH_CARD_SEVEN, DH_SUIT_CLUBS},
      {DH_CARD_KING, DH_SUIT_DIAMONDS},
      {DH_CARD_NINE, DH_SUIT_CLUBS}}},
    {{{DH_CARD_TWO, DH_SUIT_HEARTS},
      {DH_CARD_TEN, DH_SUIT_SPADES},
      {DH_CARD_TWO, DH_SUIT_CLUBS},
      {DH_CARD_THREE, DH_SUIT_DIAMONDS},
      {DH_CARD_FOUR, DH_SUIT_HEARTS}}},
};

size_t num_hands = sizeof hands / sizeof hands[0];
assert(num_hands % 3 == 0); // Make sure the total is a multiple of 3

for (size_t i = 0; i < num_hands; i += 3) {
  POKEVAL_NeedComparing need_comparing[3] = {
      {.id = 0, .hand = hands[i]},
      {.id = 1, .hand = hands[i + 1]},
      {.id = 2, .hand = hands[i + 2]},
  };

  int n_wins = POKEVAL_compare_hands(need_comparing, 3, false);
  fprintf(stderr, "winners: %d\n", n_wins);

  for (int j = 0; j < 3; ++j) {
    fprintf(stderr, "%zd: hand %d %s\n", i / 3, j, need_comparing[j].won ? "won" : "lost");
  }

  switch (i / 3) {
  case 0:
    assert(need_comparing[1].won);
    assert(!need_comparing[0].won);
    assert(!need_comparing[2].won);
    break;
  case 1:
    assert(need_comparing[0].won);
    assert(!need_comparing[1].won);
    assert(!need_comparing[2].won);
    break;
  case 2:
    assert(!need_comparing[0].won);
    assert(!need_comparing[1].won);
    assert(need_comparing[2].won);
    break;
  case 3:
    assert(need_comparing[0].won);
    assert(!need_comparing[1].won);
    assert(need_comparing[2].won);
  };
}

_MAIN_TAIL_
