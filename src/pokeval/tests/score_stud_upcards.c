#include "00_test.h"

#define CARD(f, s)                                                                                 \
  (DH_Card) { .face_val = (f), .suit = (s) }

_MAIN_HEAD_

// --- empty input ---

assert(POKEVAL_score_stud_upcards(NULL, 0) == 0);
{
  DH_Card one[1] = {CARD(DH_CARD_KING, DH_SUIT_SPADES)};
  assert(POKEVAL_score_stud_upcards(one, 0) == 0);
}

// --- single upcard: higher face scores higher ---

{
  DH_Card king[1] = {CARD(DH_CARD_KING, DH_SUIT_SPADES)};
  DH_Card seven[1] = {CARD(DH_CARD_SEVEN, DH_SUIT_SPADES)};
  assert(POKEVAL_score_stud_upcards(king, 1) > POKEVAL_score_stud_upcards(seven, 1));
}

// Ace is high: beats a King.
{
  DH_Card ace[1] = {CARD(DH_CARD_ACE, DH_SUIT_CLUBS)};
  DH_Card king[1] = {CARD(DH_CARD_KING, DH_SUIT_SPADES)};
  assert(POKEVAL_score_stud_upcards(ace, 1) > POKEVAL_score_stud_upcards(king, 1));
}

// --- pair beats any two unpaired high cards ---

{
  DH_Card pair[2] = {CARD(DH_CARD_FOUR, DH_SUIT_HEARTS), CARD(DH_CARD_FOUR, DH_SUIT_SPADES)};
  DH_Card high[2] = {CARD(DH_CARD_ACE, DH_SUIT_HEARTS), CARD(DH_CARD_KING, DH_SUIT_SPADES)};
  assert(POKEVAL_score_stud_upcards(pair, 2) > POKEVAL_score_stud_upcards(high, 2));
}

// Higher pair beats lower pair.
{
  DH_Card kk[2] = {CARD(DH_CARD_KING, DH_SUIT_HEARTS), CARD(DH_CARD_KING, DH_SUIT_SPADES)};
  DH_Card qq[2] = {CARD(DH_CARD_QUEEN, DH_SUIT_HEARTS), CARD(DH_CARD_QUEEN, DH_SUIT_SPADES)};
  assert(POKEVAL_score_stud_upcards(kk, 2) > POKEVAL_score_stud_upcards(qq, 2));
}

// --- trips beat two pair beat one pair ---

{
  DH_Card trips[3] = {CARD(DH_CARD_FIVE, DH_SUIT_HEARTS), CARD(DH_CARD_FIVE, DH_SUIT_SPADES),
                      CARD(DH_CARD_FIVE, DH_SUIT_CLUBS)};
  DH_Card two_pair[4] = {CARD(DH_CARD_ACE, DH_SUIT_HEARTS), CARD(DH_CARD_ACE, DH_SUIT_SPADES),
                         CARD(DH_CARD_KING, DH_SUIT_HEARTS), CARD(DH_CARD_KING, DH_SUIT_SPADES)};
  DH_Card one_pair[4] = {CARD(DH_CARD_ACE, DH_SUIT_HEARTS), CARD(DH_CARD_ACE, DH_SUIT_SPADES),
                         CARD(DH_CARD_KING, DH_SUIT_HEARTS), CARD(DH_CARD_QUEEN, DH_SUIT_SPADES)};
  assert(POKEVAL_score_stud_upcards(trips, 3) > POKEVAL_score_stud_upcards(two_pair, 4));
  assert(POKEVAL_score_stud_upcards(two_pair, 4) > POKEVAL_score_stud_upcards(one_pair, 4));
}

// --- suit tiebreak on an otherwise-identical top card ---
// spades(3) > hearts(2) > diamonds(1) > clubs(0)
{
  DH_Card sp[1] = {CARD(DH_CARD_KING, DH_SUIT_SPADES)};
  DH_Card he[1] = {CARD(DH_CARD_KING, DH_SUIT_HEARTS)};
  DH_Card di[1] = {CARD(DH_CARD_KING, DH_SUIT_DIAMONDS)};
  DH_Card cl[1] = {CARD(DH_CARD_KING, DH_SUIT_CLUBS)};
  uint64_t s = POKEVAL_score_stud_upcards(sp, 1);
  uint64_t h = POKEVAL_score_stud_upcards(he, 1);
  uint64_t d = POKEVAL_score_stud_upcards(di, 1);
  uint64_t c = POKEVAL_score_stud_upcards(cl, 1);
  assert(s > h && h > d && d > c);
}

// A high-card hand (even four of them) stays below any pair.
{
  DH_Card four_hi[4] = {CARD(DH_CARD_ACE, DH_SUIT_SPADES), CARD(DH_CARD_KING, DH_SUIT_HEARTS),
                        CARD(DH_CARD_QUEEN, DH_SUIT_DIAMONDS), CARD(DH_CARD_JACK, DH_SUIT_CLUBS)};
  DH_Card low_pair[2] = {CARD(DH_CARD_TWO, DH_SUIT_HEARTS), CARD(DH_CARD_TWO, DH_SUIT_SPADES)};
  assert(POKEVAL_score_stud_upcards(low_pair, 2) > POKEVAL_score_stud_upcards(four_hi, 4));
}

_MAIN_TAIL_
