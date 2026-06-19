#include "00_test.h"

/* Natural (non-wild) FLUSH and STRAIGHT_FLUSH kicker comparisons: two hands of
 * the same rank are decided high-card-down, and identical ranks tie. */

_MAIN_HEAD_

/* --- two natural flushes: last card decides --- */
/* P0: A K 9 7 4 of spades; P1: A K 9 7 3 of hearts.  Identical down to the
 * fifth card, where 4 > 3, so P0 wins. */
{
  POKEVAL_NeedComparing hands[2] = {
      {.id = 0,
       .hand = {{{DH_CARD_ACE, DH_SUIT_SPADES},
                 {DH_CARD_KING, DH_SUIT_SPADES},
                 {DH_CARD_NINE, DH_SUIT_SPADES},
                 {DH_CARD_SEVEN, DH_SUIT_SPADES},
                 {DH_CARD_FOUR, DH_SUIT_SPADES}}}},
      {.id = 1,
       .hand = {{{DH_CARD_ACE, DH_SUIT_HEARTS},
                 {DH_CARD_KING, DH_SUIT_HEARTS},
                 {DH_CARD_NINE, DH_SUIT_HEARTS},
                 {DH_CARD_SEVEN, DH_SUIT_HEARTS},
                 {DH_CARD_THREE, DH_SUIT_HEARTS}}}},
  };
  assert(POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(&hands[0].hand)) == POKEVAL_FLUSH);
  assert(POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(&hands[1].hand)) == POKEVAL_FLUSH);
  uint8_t n_wins = POKEVAL_compare_hands(hands, 2, false);
  fprintf(stderr, "natural flush kicker (A K 9 7 4 vs A K 9 7 3): %d winner(s)\n", n_wins);
  assert(n_wins == 1);
  assert(hands[0].won);
  assert(!hands[1].won);
}

/* --- identical flushes (same ranks, different suits) tie --- */
{
  POKEVAL_NeedComparing hands[2] = {
      {.id = 0,
       .hand = {{{DH_CARD_ACE, DH_SUIT_SPADES},
                 {DH_CARD_KING, DH_SUIT_SPADES},
                 {DH_CARD_NINE, DH_SUIT_SPADES},
                 {DH_CARD_SEVEN, DH_SUIT_SPADES},
                 {DH_CARD_FOUR, DH_SUIT_SPADES}}}},
      {.id = 1,
       .hand = {{{DH_CARD_ACE, DH_SUIT_DIAMONDS},
                 {DH_CARD_KING, DH_SUIT_DIAMONDS},
                 {DH_CARD_NINE, DH_SUIT_DIAMONDS},
                 {DH_CARD_SEVEN, DH_SUIT_DIAMONDS},
                 {DH_CARD_FOUR, DH_SUIT_DIAMONDS}}}},
  };
  uint8_t n_wins = POKEVAL_compare_hands(hands, 2, false);
  fprintf(stderr, "natural flush tie (identical ranks, diff suits): %d winner(s)\n", n_wins);
  assert(n_wins == 2);
  assert(hands[0].won);
  assert(hands[1].won);
}

/* --- two natural straight flushes: high straight beats low --- */
/* P0: 9-T-J-Q-K spades; P1: 5-6-7-8-9 hearts.  K-high beats 9-high. */
{
  POKEVAL_NeedComparing hands[2] = {
      {.id = 0,
       .hand = {{{DH_CARD_KING, DH_SUIT_SPADES},
                 {DH_CARD_QUEEN, DH_SUIT_SPADES},
                 {DH_CARD_JACK, DH_SUIT_SPADES},
                 {DH_CARD_TEN, DH_SUIT_SPADES},
                 {DH_CARD_NINE, DH_SUIT_SPADES}}}},
      {.id = 1,
       .hand = {{{DH_CARD_NINE, DH_SUIT_HEARTS},
                 {DH_CARD_EIGHT, DH_SUIT_HEARTS},
                 {DH_CARD_SEVEN, DH_SUIT_HEARTS},
                 {DH_CARD_SIX, DH_SUIT_HEARTS},
                 {DH_CARD_FIVE, DH_SUIT_HEARTS}}}},
  };
  assert(POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(&hands[0].hand)) == POKEVAL_STRAIGHT_FLUSH);
  assert(POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(&hands[1].hand)) == POKEVAL_STRAIGHT_FLUSH);
  uint8_t n_wins = POKEVAL_compare_hands(hands, 2, false);
  fprintf(stderr, "natural SF kicker (K-high vs 9-high): %d winner(s)\n", n_wins);
  assert(n_wins == 1);
  assert(hands[0].won);
  assert(!hands[1].won);
}

/* --- the wheel (A-2-3-4-5) straight flush is the lowest --- */
/* A 6-high straight flush must beat the wheel straight flush; the ace in the
 * wheel counts low. */
{
  POKEVAL_NeedComparing hands[2] = {
      {.id = 0,
       .hand = {{{DH_CARD_SIX, DH_SUIT_DIAMONDS},
                 {DH_CARD_FIVE, DH_SUIT_DIAMONDS},
                 {DH_CARD_FOUR, DH_SUIT_DIAMONDS},
                 {DH_CARD_THREE, DH_SUIT_DIAMONDS},
                 {DH_CARD_TWO, DH_SUIT_DIAMONDS}}}},
      {.id = 1,
       .hand = {{{DH_CARD_ACE, DH_SUIT_CLUBS},
                 {DH_CARD_TWO, DH_SUIT_CLUBS},
                 {DH_CARD_THREE, DH_SUIT_CLUBS},
                 {DH_CARD_FOUR, DH_SUIT_CLUBS},
                 {DH_CARD_FIVE, DH_SUIT_CLUBS}}}},
  };
  assert(POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(&hands[0].hand)) == POKEVAL_STRAIGHT_FLUSH);
  assert(POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(&hands[1].hand)) == POKEVAL_STRAIGHT_FLUSH);
  uint8_t n_wins = POKEVAL_compare_hands(hands, 2, false);
  fprintf(stderr, "natural SF wheel (6-high vs A-2-3-4-5): %d winner(s)\n", n_wins);
  assert(n_wins == 1);
  assert(hands[0].won);  /* 6-high wins */
  assert(!hands[1].won); /* wheel is lowest */
}

_MAIN_TAIL_
