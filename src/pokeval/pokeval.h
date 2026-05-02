/*
 pokeval.h

 MIT License

 Copyright (c) 2025 Andy Alt

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

#ifndef __POKEVAL_H
#define __POKEVAL_H

#include <stdbool.h>
#include <stddef.h>

#include "deckhandler.h"

#define POKEVAL_HAND_SIZE 5

#define POKEVAL_ACE (DH_CARD_KING + 1)

typedef enum {
  POKEVAL_HIGH_CARD,
  POKEVAL_PAIR,
  POKEVAL_TWO_PAIR,
  POKEVAL_THREE_OF_A_KIND,
  POKEVAL_STRAIGHT,
  POKEVAL_FLUSH,
  POKEVAL_FULL_HOUSE,
  POKEVAL_FOUR_OF_A_KIND,
  POKEVAL_STRAIGHT_FLUSH,
  POKEVAL_ROYAL_FLUSH,
  POKEVAL_FIVE_OF_A_KIND,
  NUM_HAND_RANKS
} hand_rank_t;

extern const char *POKEVAL_rank[NUM_HAND_RANKS];

typedef struct {
  DH_Card card[5];
} POKEVAL_Hand_5;

typedef struct {
  DH_Card card[9];
} POKEVAL_Hand_9;

typedef struct {
  bool won;
  int8_t id;
  POKEVAL_Hand_9 hand;
  POKEVAL_Hand_5 hand_5;
} POKEVAL_NeedComparing;

void POKEVAL_sort_hand(POKEVAL_Hand_5 *hand);

void POKEVAL_sort_hand_lowball(POKEVAL_Hand_5 *hand);

short POKEVAL_evaluate_hand(POKEVAL_Hand_5 hand);

// Returns the number of winners and fills `winners` with their indices.
// `winners` must have at least `count` elements allocated by the caller.
uint8_t POKEVAL_compare_hands(POKEVAL_NeedComparing *hands, const uint8_t count,
                              const bool lowball);

POKEVAL_Hand_5 POKEVAL_hand5_from_hand7(const POKEVAL_Hand_9 *src);

// Omaha evaluation: cards 0-3 are hole cards, cards 4-8 are community cards.
// Tries all C(4,2) x C(5,3) = 60 combinations and returns the best hand using
// exactly 2 hole cards and 3 community cards.
POKEVAL_Hand_5 POKEVAL_hand5_omaha(const POKEVAL_Hand_9 *src);
uint8_t POKEVAL_compare_hands_omaha(POKEVAL_NeedComparing *hands, uint8_t count);

// Wild card variants: `wild_face` is the face value treated as wild (e.g. DH_CARD_TWO).
short POKEVAL_evaluate_hand_wild(POKEVAL_Hand_5 hand, int32_t wild_face);
POKEVAL_Hand_5 POKEVAL_hand5_from_hand7_wild(const POKEVAL_Hand_9 *src, int32_t wild_face);
uint8_t POKEVAL_compare_hands_wild(POKEVAL_NeedComparing *hands, uint8_t count, int32_t wild_face);

// Bring-in ordering for stud games.
// Suit rank: clubs=0 (lowest, forced to bring in) < diamonds=1 < hearts=2 < spades=3 (highest).
int POKEVAL_suit_bringin_rank(int32_t suit);

// Returns true if card a must post the bring-in before card b.
// Lower face value (ace-high) brings in first; ties broken by suit rank (clubs loses).
bool POKEVAL_card_bringin_lt(DH_Card a, DH_Card b);

// Score up to 4 visible (face-up) stud cards for street-opening order.
// Higher score = better visible hand = acts first on subsequent streets.
// n must be in [0, 4]. Returns 0 for empty input.
uint64_t POKEVAL_score_stud_upcards(const DH_Card *cards, int n);

// Score 1-7 visible cards for betting-order comparison in no-peek games.
// For n <= 4 uses the same encoding as POKEVAL_score_stud_upcards but WITHOUT
// a suit tiebreaker, so equal-rank hands score identically (neither beats the
// other, forcing the player to flip another card).
// For n in [5, 7] uses full hand evaluation.
// Higher score = better visible hand = must beat to stop flipping.
uint64_t POKEVAL_score_visible_cards(const DH_Card *cards, int n);

#endif
