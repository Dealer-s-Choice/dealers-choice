#include "00_test.h"

#define TEST_WILD(expected_rank, f, s)                                                             \
  do {                                                                                             \
    int tw_faces[POKEVAL_HAND_SIZE];                                                               \
    int tw_suits[POKEVAL_HAND_SIZE];                                                               \
    static POKEVAL_Hand_5 hand;                                                                    \
    memcpy(tw_faces, f, POKEVAL_HAND_SIZE * sizeof(int));                                          \
    memcpy(tw_suits, s, POKEVAL_HAND_SIZE * sizeof(int));                                          \
    set_hand(&hand, tw_faces, tw_suits);                                                           \
    short rank = POKEVAL_evaluate_hand_wild(hand, DH_CARD_TWO);                                    \
    fprintf(stderr, "rank: %s\n", POKEVAL_rank[rank]);                                             \
    assert(rank == expected_rank);                                                                 \
  } while (0)

_MAIN_HEAD_

/* --- No wilds: should match regular evaluate_hand --- */
{
  int fh_f[] = {DH_CARD_ACE, DH_CARD_ACE, DH_CARD_KING, DH_CARD_KING, DH_CARD_KING};
  int fh_s[] = {DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_DIAMONDS};
  TEST_WILD(POKEVAL_FULL_HOUSE, fh_f, fh_s);
}

/* --- 1 wild: pair + wild = three-of-a-kind --- */
{
  int f[] = {DH_CARD_KING, DH_CARD_KING, DH_CARD_TWO, DH_CARD_NINE, DH_CARD_THREE};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_SPADES, DH_SUIT_DIAMONDS, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_THREE_OF_A_KIND, f, s);
}

/* --- 1 wild: three-of-a-kind + wild = four-of-a-kind --- */
{
  int f[] = {DH_CARD_QUEEN, DH_CARD_QUEEN, DH_CARD_QUEEN, DH_CARD_TWO, DH_CARD_FIVE};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_DIAMONDS, DH_SUIT_SPADES, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_FOUR_OF_A_KIND, f, s);
}

/* --- 1 wild: four-of-a-kind + wild = five-of-a-kind --- */
{
  int f[] = {DH_CARD_JACK, DH_CARD_JACK, DH_CARD_JACK, DH_CARD_JACK, DH_CARD_TWO};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_DIAMONDS, DH_SUIT_SPADES, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_FIVE_OF_A_KIND, f, s);
}

/* --- 1 wild: 4 suited consecutive + wild = straight flush --- */
{
  int f[] = {DH_CARD_FIVE, DH_CARD_SIX, DH_CARD_SEVEN, DH_CARD_EIGHT, DH_CARD_TWO};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_SPADES};
  TEST_WILD(POKEVAL_STRAIGHT_FLUSH, f, s);
}

/* --- 1 wild: A,K,Q,J same suit + wild = royal flush --- */
{
  int f[] = {DH_CARD_ACE, DH_CARD_KING, DH_CARD_QUEEN, DH_CARD_JACK, DH_CARD_TWO};
  int s[] = {DH_SUIT_SPADES, DH_SUIT_SPADES, DH_SUIT_SPADES, DH_SUIT_SPADES, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_ROYAL_FLUSH, f, s);
}

/* --- 1 wild: 4 consecutive (off-suit) + wild = straight --- */
{
  int f[] = {DH_CARD_NINE, DH_CARD_TEN, DH_CARD_JACK, DH_CARD_QUEEN, DH_CARD_TWO};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_DIAMONDS, DH_SUIT_SPADES, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_STRAIGHT, f, s);
}

/* --- 1 wild: 4 same suit (non-sequential) + wild = flush --- */
{
  int f[] = {DH_CARD_THREE, DH_CARD_SIX, DH_CARD_NINE, DH_CARD_QUEEN, DH_CARD_TWO};
  int s[] = {DH_SUIT_CLUBS, DH_SUIT_CLUBS, DH_SUIT_CLUBS, DH_SUIT_CLUBS, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_FLUSH, f, s);
}

/* --- 2 wilds: pair + 2 wilds = four-of-a-kind --- */
{
  int f[] = {DH_CARD_TEN, DH_CARD_TEN, DH_CARD_TWO, DH_CARD_TWO, DH_CARD_FIVE};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_SPADES, DH_SUIT_DIAMONDS, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_FOUR_OF_A_KIND, f, s);
}

/* --- 2 wilds: three-of-a-kind + 2 wilds = five-of-a-kind --- */
{
  int f[] = {DH_CARD_ACE, DH_CARD_ACE, DH_CARD_ACE, DH_CARD_TWO, DH_CARD_TWO};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_DIAMONDS, DH_SUIT_SPADES, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_FIVE_OF_A_KIND, f, s);
}

/* --- 2 wilds: 3 suited consecutive + 2 wilds = straight flush --- */
{
  int f[] = {DH_CARD_THREE, DH_CARD_FOUR, DH_CARD_FIVE, DH_CARD_TWO, DH_CARD_TWO};
  int s[] = {DH_SUIT_DIAMONDS, DH_SUIT_DIAMONDS, DH_SUIT_DIAMONDS, DH_SUIT_HEARTS, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_STRAIGHT_FLUSH, f, s);
}

/* --- 3 wilds: two different-value real cards + 3 wilds → four-of-a-kind --- */
{
  /* Real: K♥ and 7♥ are different, so can't be five-of-a-kind; best is four kings */
  int f[] = {DH_CARD_KING, DH_CARD_TWO, DH_CARD_TWO, DH_CARD_TWO, DH_CARD_SEVEN};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_SPADES, DH_SUIT_CLUBS, DH_SUIT_DIAMONDS, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_FOUR_OF_A_KIND, f, s);
}

/* --- 3 wilds: two same-value real cards + 3 wilds = five-of-a-kind --- */
{
  int f[] = {DH_CARD_EIGHT, DH_CARD_EIGHT, DH_CARD_TWO, DH_CARD_TWO, DH_CARD_TWO};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_SPADES, DH_SUIT_DIAMONDS, DH_SUIT_HEARTS};
  TEST_WILD(POKEVAL_FIVE_OF_A_KIND, f, s);
}

/* --- 4 wilds + 1 real card = five-of-a-kind --- */
{
  int f[] = {DH_CARD_KING, DH_CARD_TWO, DH_CARD_TWO, DH_CARD_TWO, DH_CARD_TWO};
  int s[] = {DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_DIAMONDS, DH_SUIT_SPADES};
  TEST_WILD(POKEVAL_FIVE_OF_A_KIND, f, s);
}

/* --- 7-card hand evaluator with wilds --- */
{
  /* 3 kings + 2 wilds (among 7 cards) → best 5-card hand is five-of-a-kind of kings */
  POKEVAL_Hand_9 h7 = {{{DH_CARD_KING, DH_SUIT_HEARTS},
                        {DH_CARD_KING, DH_SUIT_CLUBS},
                        {DH_CARD_KING, DH_SUIT_DIAMONDS},
                        {DH_CARD_TWO, DH_SUIT_SPADES},
                        {DH_CARD_TWO, DH_SUIT_HEARTS},
                        {DH_CARD_NINE, DH_SUIT_CLUBS},
                        {DH_CARD_THREE, DH_SUIT_DIAMONDS}}};
  POKEVAL_Hand_5 best = POKEVAL_hand5_from_hand7_wild(&h7, DH_CARD_TWO);
  short rank = POKEVAL_evaluate_hand_wild(best, DH_CARD_TWO);
  fprintf(stderr, "7-card wild best: %s\n", POKEVAL_rank[rank]);
  assert(rank == POKEVAL_FIVE_OF_A_KIND);
}

{
  /* 4 suited consecutive + 1 wild in 7-card hand → should find straight flush */
  POKEVAL_Hand_9 h7 = {{{DH_CARD_FIVE, DH_SUIT_HEARTS},
                        {DH_CARD_SIX, DH_SUIT_HEARTS},
                        {DH_CARD_SEVEN, DH_SUIT_HEARTS},
                        {DH_CARD_EIGHT, DH_SUIT_HEARTS},
                        {DH_CARD_TWO, DH_SUIT_SPADES},
                        {DH_CARD_QUEEN, DH_SUIT_CLUBS},
                        {DH_CARD_THREE, DH_SUIT_DIAMONDS}}};
  POKEVAL_Hand_5 best = POKEVAL_hand5_from_hand7_wild(&h7, DH_CARD_TWO);
  short rank = POKEVAL_evaluate_hand_wild(best, DH_CARD_TWO);
  fprintf(stderr, "7-card straight flush wild: %s\n", POKEVAL_rank[rank]);
  assert(rank == POKEVAL_STRAIGHT_FLUSH);
}

/* --- 6-card wild hands --- */

{
  /* Wild (2) is the 6th (last) card; no wilds in first 5.
   * K K K A 9 2 → best 5: K K K A 2 = four kings (wild fills 4th K) + ace kicker */
  POKEVAL_Hand_9 h6 = {{{DH_CARD_KING, DH_SUIT_SPADES},
                        {DH_CARD_KING, DH_SUIT_HEARTS},
                        {DH_CARD_KING, DH_SUIT_DIAMONDS},
                        {DH_CARD_ACE, DH_SUIT_CLUBS},
                        {DH_CARD_NINE, DH_SUIT_SPADES},
                        {DH_CARD_TWO, DH_SUIT_HEARTS}}};
  POKEVAL_Hand_5 best = POKEVAL_hand5_from_hand7_wild(&h6, DH_CARD_TWO);
  short rank = POKEVAL_evaluate_hand_wild(best, DH_CARD_TWO);
  fprintf(stderr, "6-card wild (2 last): %s\n", POKEVAL_rank[rank]);
  assert(rank == POKEVAL_FOUR_OF_A_KIND);
}

{
  /* Wild (2) in position 3 of first 5, another wild (2) as the 6th card.
   * J J J 2 6 2 → two wilds + three jacks = five of a kind */
  POKEVAL_Hand_9 h6 = {{{DH_CARD_JACK, DH_SUIT_SPADES},
                        {DH_CARD_JACK, DH_SUIT_HEARTS},
                        {DH_CARD_JACK, DH_SUIT_DIAMONDS},
                        {DH_CARD_TWO, DH_SUIT_CLUBS},
                        {DH_CARD_SIX, DH_SUIT_SPADES},
                        {DH_CARD_TWO, DH_SUIT_HEARTS}}};
  POKEVAL_Hand_5 best = POKEVAL_hand5_from_hand7_wild(&h6, DH_CARD_TWO);
  short rank = POKEVAL_evaluate_hand_wild(best, DH_CARD_TWO);
  fprintf(stderr, "6-card wild (2 in first 5 and last): %s\n", POKEVAL_rank[rank]);
  assert(rank == POKEVAL_FIVE_OF_A_KIND);
}

{
  /* Wild (2) in position 2 of first 5, another wild (2) as the 6th card; straight flush.
   * 5♣ 6♣ 2♥ 8♣ 9♣ 2♦ → best 5 using one wild as 7♣: 5-6-7-8-9 of clubs */
  POKEVAL_Hand_9 h6 = {{{DH_CARD_FIVE, DH_SUIT_CLUBS},
                        {DH_CARD_SIX, DH_SUIT_CLUBS},
                        {DH_CARD_TWO, DH_SUIT_HEARTS},
                        {DH_CARD_EIGHT, DH_SUIT_CLUBS},
                        {DH_CARD_NINE, DH_SUIT_CLUBS},
                        {DH_CARD_TWO, DH_SUIT_DIAMONDS}}};
  POKEVAL_Hand_5 best = POKEVAL_hand5_from_hand7_wild(&h6, DH_CARD_TWO);
  short rank = POKEVAL_evaluate_hand_wild(best, DH_CARD_TWO);
  fprintf(stderr, "6-card wild (2 in first 5 and last, straight flush): %s\n", POKEVAL_rank[rank]);
  assert(rank == POKEVAL_STRAIGHT_FLUSH);
}

/* --- more 7-card wild hands --- */

{
  /* Wild (2) is the 7th (last) card; no wilds in first 6.
   * Q Q Q A K 4 2 → best 5: Q Q Q A 2 = four queens (wild) + ace kicker */
  POKEVAL_Hand_9 h7 = {{{DH_CARD_QUEEN, DH_SUIT_SPADES},
                        {DH_CARD_QUEEN, DH_SUIT_HEARTS},
                        {DH_CARD_QUEEN, DH_SUIT_DIAMONDS},
                        {DH_CARD_ACE, DH_SUIT_CLUBS},
                        {DH_CARD_KING, DH_SUIT_SPADES},
                        {DH_CARD_FOUR, DH_SUIT_HEARTS},
                        {DH_CARD_TWO, DH_SUIT_CLUBS}}};
  POKEVAL_Hand_5 best = POKEVAL_hand5_from_hand7_wild(&h7, DH_CARD_TWO);
  short rank = POKEVAL_evaluate_hand_wild(best, DH_CARD_TWO);
  fprintf(stderr, "7-card wild (2 last): %s\n", POKEVAL_rank[rank]);
  assert(rank == POKEVAL_FOUR_OF_A_KIND);
}

{
  /* Wild (2) in position 2 of first 5, another wild (2) as the 7th card.
   * A A 2 A K 7 2 → three aces + two wilds = five of a kind */
  POKEVAL_Hand_9 h7 = {{{DH_CARD_ACE, DH_SUIT_SPADES},
                        {DH_CARD_ACE, DH_SUIT_HEARTS},
                        {DH_CARD_TWO, DH_SUIT_DIAMONDS},
                        {DH_CARD_ACE, DH_SUIT_CLUBS},
                        {DH_CARD_KING, DH_SUIT_SPADES},
                        {DH_CARD_SEVEN, DH_SUIT_HEARTS},
                        {DH_CARD_TWO, DH_SUIT_CLUBS}}};
  POKEVAL_Hand_5 best = POKEVAL_hand5_from_hand7_wild(&h7, DH_CARD_TWO);
  short rank = POKEVAL_evaluate_hand_wild(best, DH_CARD_TWO);
  fprintf(stderr, "7-card wild (2 in first 5 and last): %s\n", POKEVAL_rank[rank]);
  assert(rank == POKEVAL_FIVE_OF_A_KIND);
}

{
  /* Wild (2) in position 4 of first 5, another wild (2) as the 7th card; straight flush.
   * 7♥ 8♥ 9♥ T♥ 2♠ K♣ 2♦ → best 5: 7-8-9-T-J of hearts (wild fills J♥) */
  POKEVAL_Hand_9 h7 = {{{DH_CARD_SEVEN, DH_SUIT_HEARTS},
                        {DH_CARD_EIGHT, DH_SUIT_HEARTS},
                        {DH_CARD_NINE, DH_SUIT_HEARTS},
                        {DH_CARD_TEN, DH_SUIT_HEARTS},
                        {DH_CARD_TWO, DH_SUIT_SPADES},
                        {DH_CARD_KING, DH_SUIT_CLUBS},
                        {DH_CARD_TWO, DH_SUIT_DIAMONDS}}};
  POKEVAL_Hand_5 best = POKEVAL_hand5_from_hand7_wild(&h7, DH_CARD_TWO);
  short rank = POKEVAL_evaluate_hand_wild(best, DH_CARD_TWO);
  fprintf(stderr, "7-card wild (2 in first 5 and last, straight flush): %s\n", POKEVAL_rank[rank]);
  assert(rank == POKEVAL_STRAIGHT_FLUSH);
}

/* --- compare_hands_wild: trips comparison ---
 * J J 2 3 4 (wild 2 = three Jacks) must beat 7 7 7 K Q (three Sevens) even
 * though K Q sort higher than J in a raw high-card comparison.
 * Regression test: compare_hands_wild must use rank-aware tiebreaking, not
 * raw compare_high_cards, when both hands evaluate to the same rank.
 *
 * PAD_NULL_CARDS after the 5 real cards is required so that
 * hand5_from_hand7_wild stops at n=5 and takes the direct copy path instead
 * of the combo path, which would otherwise treat zero-initialized trailing
 * slots as a phantom pair and inflate both hands to Full House. */
{
  POKEVAL_NeedComparing hands[2] = {
    /* Hand 0: 7 7 7 K Q — three Sevens with high kickers */
    {.id = 0,
     .hand = {{{DH_CARD_SEVEN, DH_SUIT_HEARTS},
               {DH_CARD_SEVEN, DH_SUIT_DIAMONDS},
               {DH_CARD_SEVEN, DH_SUIT_SPADES},
               {DH_CARD_KING, DH_SUIT_CLUBS},
               {DH_CARD_QUEEN, DH_SUIT_CLUBS},
               PAD_NULL_CARDS}}},
    /* Hand 1: J J 2 3 4 — wild 2 fills the third Jack */
    {.id = 1,
     .hand = {{{DH_CARD_JACK, DH_SUIT_HEARTS},
               {DH_CARD_JACK, DH_SUIT_DIAMONDS},
               {DH_CARD_TWO, DH_SUIT_SPADES},
               {DH_CARD_THREE, DH_SUIT_CLUBS},
               {DH_CARD_FOUR, DH_SUIT_HEARTS},
               PAD_NULL_CARDS}}},
  };
  uint8_t n_wins = POKEVAL_compare_hands_wild(hands, 2, DH_CARD_TWO);
  fprintf(stderr, "wild trips compare (J J 2 3 4 vs 7 7 7 K Q): %d winner(s)\n", n_wins);
  assert(n_wins == 1);
  assert(!hands[0].won); /* three Sevens loses */
  assert(hands[1].won);  /* three Jacks (wild) wins */
}

_MAIN_TAIL_
