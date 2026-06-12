#include "00_test.h"

// California lowball (ace-to-five): ace is low, straights and flushes do not
// count against a hand, lowest hand wins. Best possible hand is the wheel
// (5-4-3-2-A).

// Build a Hand_9 holding exactly five cards (lowball is a 5-card draw game).
#define LOW_HAND(c0f, c0s, c1f, c1s, c2f, c2s, c3f, c3s, c4f, c4s)                                 \
  {{{c0f, c0s}, {c1f, c1s}, {c2f, c2s}, {c3f, c3s}, {c4f, c4s}, PAD_NULL_CARDS}}

static int compare2(POKEVAL_Hand_9 a, POKEVAL_Hand_9 b, uint8_t *n_wins) {
  POKEVAL_NeedComparing nc[2] = {
      {.id = 0, .hand = a},
      {.id = 1, .hand = b},
  };
  *n_wins = POKEVAL_compare_hands(nc, 2, true);
  if (*n_wins == 2)
    return -1; // tie
  return nc[0].won ? 0 : 1;
}

_MAIN_HEAD_

// --- sort_hand_lowball: ascending with ace low ---
{
  POKEVAL_Hand_5 hand = {.card = {
                             {DH_CARD_TEN, DH_SUIT_HEARTS},
                             {DH_CARD_THREE, DH_SUIT_CLUBS},
                             {DH_CARD_ACE, DH_SUIT_DIAMONDS},
                             {DH_CARD_FIVE, DH_SUIT_SPADES},
                             {DH_CARD_KING, DH_SUIT_CLUBS},
                         }};
  POKEVAL_sort_hand_lowball(&hand);
  int expected[] = {DH_CARD_ACE, DH_CARD_THREE, DH_CARD_FIVE, DH_CARD_TEN, DH_CARD_KING};
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
    fprintf(stderr, "card: %d | ", hand.card[i].face_val);
    assert(hand.card[i].face_val == expected[i]);
  }
  fputc('\n', stderr);
}

uint8_t n_wins;
int winner;

// --- Wheel (5-4-3-2-A) beats 6-low; the straight does not count against ---
{
  POKEVAL_Hand_9 wheel = LOW_HAND(DH_CARD_FIVE, DH_SUIT_HEARTS, DH_CARD_FOUR, DH_SUIT_CLUBS,
                                  DH_CARD_THREE, DH_SUIT_DIAMONDS, DH_CARD_TWO, DH_SUIT_SPADES,
                                  DH_CARD_ACE, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 six_low = LOW_HAND(DH_CARD_SIX, DH_SUIT_HEARTS, DH_CARD_FOUR, DH_SUIT_CLUBS,
                                    DH_CARD_THREE, DH_SUIT_DIAMONDS, DH_CARD_TWO, DH_SUIT_SPADES,
                                    DH_CARD_ACE, DH_SUIT_CLUBS);
  winner = compare2(wheel, six_low, &n_wins);
  fprintf(stderr, "wheel vs 6-low: winner=%d\n", winner);
  assert(n_wins == 1 && winner == 0);
}

// --- Flush does not count against: suited 8-6 low beats offsuit 8-7 low ---
{
  POKEVAL_Hand_9 suited = LOW_HAND(DH_CARD_EIGHT, DH_SUIT_HEARTS, DH_CARD_SIX, DH_SUIT_HEARTS,
                                   DH_CARD_FOUR, DH_SUIT_HEARTS, DH_CARD_THREE, DH_SUIT_HEARTS,
                                   DH_CARD_TWO, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 offsuit = LOW_HAND(DH_CARD_EIGHT, DH_SUIT_SPADES, DH_CARD_SEVEN, DH_SUIT_CLUBS,
                                    DH_CARD_FOUR, DH_SUIT_DIAMONDS, DH_CARD_THREE, DH_SUIT_SPADES,
                                    DH_CARD_TWO, DH_SUIT_CLUBS);
  winner = compare2(suited, offsuit, &n_wins);
  fprintf(stderr, "suited 8-6 low vs offsuit 8-7 low: winner=%d\n", winner);
  assert(n_wins == 1 && winner == 0);
}

// --- Any no-pair hand beats any paired hand ---
{
  POKEVAL_Hand_9 king_high = LOW_HAND(DH_CARD_KING, DH_SUIT_HEARTS, DH_CARD_QUEEN, DH_SUIT_CLUBS,
                                      DH_CARD_JACK, DH_SUIT_DIAMONDS, DH_CARD_NINE, DH_SUIT_SPADES,
                                      DH_CARD_EIGHT, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 low_pair = LOW_HAND(DH_CARD_TWO, DH_SUIT_HEARTS, DH_CARD_TWO, DH_SUIT_CLUBS,
                                     DH_CARD_THREE, DH_SUIT_DIAMONDS, DH_CARD_FOUR, DH_SUIT_SPADES,
                                     DH_CARD_FIVE, DH_SUIT_HEARTS);
  winner = compare2(king_high, low_pair, &n_wins);
  fprintf(stderr, "K-high no pair vs pair of twos: winner=%d\n", winner);
  assert(n_wins == 1 && winner == 0);
}

// --- Pair vs pair: the lower pair wins ---
{
  POKEVAL_Hand_9 pair_twos = LOW_HAND(DH_CARD_TWO, DH_SUIT_HEARTS, DH_CARD_TWO, DH_SUIT_CLUBS,
                                      DH_CARD_ACE, DH_SUIT_DIAMONDS, DH_CARD_THREE, DH_SUIT_SPADES,
                                      DH_CARD_FOUR, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 pair_threes = LOW_HAND(DH_CARD_THREE, DH_SUIT_HEARTS, DH_CARD_THREE, DH_SUIT_CLUBS,
                                        DH_CARD_ACE, DH_SUIT_CLUBS, DH_CARD_TWO, DH_SUIT_DIAMONDS,
                                        DH_CARD_FOUR, DH_SUIT_CLUBS);
  winner = compare2(pair_twos, pair_threes, &n_wins);
  fprintf(stderr, "pair of twos vs pair of threes: winner=%d\n", winner);
  assert(n_wins == 1 && winner == 0);
}

// --- Ace plays low in card-by-card comparison: 9-5-4-3-A beats 9-5-4-3-2 ---
{
  POKEVAL_Hand_9 with_ace = LOW_HAND(DH_CARD_NINE, DH_SUIT_HEARTS, DH_CARD_FIVE, DH_SUIT_CLUBS,
                                     DH_CARD_FOUR, DH_SUIT_DIAMONDS, DH_CARD_THREE, DH_SUIT_SPADES,
                                     DH_CARD_ACE, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 with_two = LOW_HAND(DH_CARD_NINE, DH_SUIT_SPADES, DH_CARD_FIVE, DH_SUIT_DIAMONDS,
                                     DH_CARD_FOUR, DH_SUIT_CLUBS, DH_CARD_THREE, DH_SUIT_HEARTS,
                                     DH_CARD_TWO, DH_SUIT_SPADES);
  winner = compare2(with_ace, with_two, &n_wins);
  fprintf(stderr, "9543A vs 95432: winner=%d\n", winner);
  assert(n_wins == 1 && winner == 0);
}

// --- Identical ranks, different suits: tie, both win ---
{
  POKEVAL_Hand_9 a = LOW_HAND(DH_CARD_SEVEN, DH_SUIT_HEARTS, DH_CARD_FIVE, DH_SUIT_CLUBS,
                              DH_CARD_FOUR, DH_SUIT_DIAMONDS, DH_CARD_THREE, DH_SUIT_SPADES,
                              DH_CARD_TWO, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 b = LOW_HAND(DH_CARD_SEVEN, DH_SUIT_SPADES, DH_CARD_FIVE, DH_SUIT_DIAMONDS,
                              DH_CARD_FOUR, DH_SUIT_CLUBS, DH_CARD_THREE, DH_SUIT_HEARTS,
                              DH_CARD_TWO, DH_SUIT_SPADES);
  winner = compare2(a, b, &n_wins);
  fprintf(stderr, "identical 7-lows: n_wins=%d\n", n_wins);
  assert(n_wins == 2);
}

// --- Trips lose to two pair ---
{
  POKEVAL_Hand_9 trips = LOW_HAND(DH_CARD_TWO, DH_SUIT_HEARTS, DH_CARD_TWO, DH_SUIT_CLUBS,
                                  DH_CARD_TWO, DH_SUIT_DIAMONDS, DH_CARD_THREE, DH_SUIT_SPADES,
                                  DH_CARD_FOUR, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 two_pair = LOW_HAND(DH_CARD_ACE, DH_SUIT_HEARTS, DH_CARD_ACE, DH_SUIT_CLUBS,
                                     DH_CARD_THREE, DH_SUIT_DIAMONDS, DH_CARD_THREE, DH_SUIT_CLUBS,
                                     DH_CARD_FOUR, DH_SUIT_SPADES);
  winner = compare2(trips, two_pair, &n_wins);
  fprintf(stderr, "trips vs two pair: %s wins\n", winner == 1 ? "two pair" : "trips");
  assert(n_wins == 1 && winner == 1); /* two pair beats trips */
}

// --- Two pair loses to one pair ---
{
  POKEVAL_Hand_9 two_pair = LOW_HAND(DH_CARD_ACE, DH_SUIT_HEARTS, DH_CARD_ACE, DH_SUIT_CLUBS,
                                     DH_CARD_THREE, DH_SUIT_DIAMONDS, DH_CARD_THREE, DH_SUIT_CLUBS,
                                     DH_CARD_FIVE, DH_SUIT_SPADES);
  POKEVAL_Hand_9 one_pair = LOW_HAND(DH_CARD_KING, DH_SUIT_HEARTS, DH_CARD_KING, DH_SUIT_CLUBS,
                                     DH_CARD_FOUR, DH_SUIT_DIAMONDS, DH_CARD_FIVE, DH_SUIT_HEARTS,
                                     DH_CARD_SIX, DH_SUIT_SPADES);
  winner = compare2(two_pair, one_pair, &n_wins);
  fprintf(stderr, "two pair vs one pair: %s wins\n", winner == 1 ? "one pair" : "two pair");
  assert(n_wins == 1 && winner == 1); /* one pair beats two pair */
}

// --- Same class: the pair value decides before the kickers ---
// Pair of twos with big kickers still beats pair of threes with small kickers.
{
  POKEVAL_Hand_9 twos_big_kick = LOW_HAND(
      DH_CARD_TWO, DH_SUIT_HEARTS, DH_CARD_TWO, DH_SUIT_CLUBS, DH_CARD_KING, DH_SUIT_DIAMONDS,
      DH_CARD_QUEEN, DH_SUIT_SPADES, DH_CARD_JACK, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 threes_small_kick = LOW_HAND(
      DH_CARD_THREE, DH_SUIT_HEARTS, DH_CARD_THREE, DH_SUIT_CLUBS, DH_CARD_ACE, DH_SUIT_DIAMONDS,
      DH_CARD_FOUR, DH_SUIT_SPADES, DH_CARD_FIVE, DH_SUIT_CLUBS);
  winner = compare2(twos_big_kick, threes_small_kick, &n_wins);
  fprintf(stderr, "2-2-K-Q-J vs 3-3-A-4-5: %s wins\n", winner == 0 ? "pair of twos" : "pair of threes");
  assert(n_wins == 1 && winner == 0); /* lower pair wins despite big kickers */
}

// --- Two pair vs two pair: lower top pair wins ---
{
  POKEVAL_Hand_9 threes_up = LOW_HAND(DH_CARD_THREE, DH_SUIT_HEARTS, DH_CARD_THREE, DH_SUIT_CLUBS,
                                      DH_CARD_TWO, DH_SUIT_DIAMONDS, DH_CARD_TWO, DH_SUIT_SPADES,
                                      DH_CARD_KING, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 fours_up = LOW_HAND(DH_CARD_FOUR, DH_SUIT_HEARTS, DH_CARD_FOUR, DH_SUIT_CLUBS,
                                     DH_CARD_ACE, DH_SUIT_DIAMONDS, DH_CARD_ACE, DH_SUIT_SPADES,
                                     DH_CARD_FIVE, DH_SUIT_HEARTS);
  winner = compare2(threes_up, fours_up, &n_wins);
  fprintf(stderr, "3-3-2-2-K vs 4-4-A-A-5: %s wins\n", winner == 0 ? "threes up" : "fours up");
  assert(n_wins == 1 && winner == 0); /* lower top pair wins despite K kicker */
}

// --- Full house loses to trips ---
{
  POKEVAL_Hand_9 boat = LOW_HAND(DH_CARD_TWO, DH_SUIT_HEARTS, DH_CARD_TWO, DH_SUIT_CLUBS,
                                 DH_CARD_TWO, DH_SUIT_DIAMONDS, DH_CARD_THREE, DH_SUIT_SPADES,
                                 DH_CARD_THREE, DH_SUIT_HEARTS);
  POKEVAL_Hand_9 trips = LOW_HAND(DH_CARD_FOUR, DH_SUIT_HEARTS, DH_CARD_FOUR, DH_SUIT_CLUBS,
                                  DH_CARD_FOUR, DH_SUIT_DIAMONDS, DH_CARD_ACE, DH_SUIT_SPADES,
                                  DH_CARD_TWO, DH_SUIT_SPADES);
  winner = compare2(boat, trips, &n_wins);
  fprintf(stderr, "full house vs trips: %s wins\n", winner == 1 ? "trips" : "full house");
  assert(n_wins == 1 && winner == 1); /* trips beat a full house */
}

_MAIN_TAIL_
