/*
 pokeval.c

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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pokeval.h"

const char *POKEVAL_rank[NUM_HAND_RANKS] = {[POKEVAL_HIGH_CARD] = "High Card",
                                            [POKEVAL_PAIR] = "Pair",
                                            [POKEVAL_TWO_PAIR] = "Two Pair",
                                            [POKEVAL_THREE_OF_A_KIND] = "Three-of-a-Kind",
                                            [POKEVAL_STRAIGHT] = "Straight",
                                            [POKEVAL_FLUSH] = "Flush",
                                            [POKEVAL_FULL_HOUSE] = "Full House",
                                            [POKEVAL_FOUR_OF_A_KIND] = "Four-of-a-Kind",
                                            [POKEVAL_STRAIGHT_FLUSH] = "Straight Flush",
                                            [POKEVAL_FIVE_OF_A_KIND] = "Five-of-a-Kind",
                                            [POKEVAL_ROYAL_FLUSH] = "Royal Flush"};

static inline int face_rank(int val) { return (val == DH_CARD_ACE) ? 14 : val; }

static int count_face(const POKEVAL_Hand_5 *hand, int face_val) {
  int count = 0;
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i)
    if (hand->card[i].face_val == face_val)
      count++;
  return count;
}

void POKEVAL_sort_hand(POKEVAL_Hand_5 *hand) {
  for (int i = 0; i < POKEVAL_HAND_SIZE - 1; ++i) {
    for (int j = i + 1; j < POKEVAL_HAND_SIZE; ++j) {
      int32_t val_i =
          (hand->card[i].face_val == DH_CARD_ACE) ? POKEVAL_ACE : hand->card[i].face_val;
      int32_t val_j =
          (hand->card[j].face_val == DH_CARD_ACE) ? POKEVAL_ACE : hand->card[j].face_val;
      hand->card[i].face_val = val_i;
      hand->card[j].face_val = val_j;

      if (val_i < val_j) {
        DH_Card tmp = hand->card[i];
        hand->card[i] = hand->card[j];
        hand->card[j] = tmp;
      }
    }
  }
}

static bool is_straight(POKEVAL_Hand_5 *hand) {
  int faces[POKEVAL_HAND_SIZE];
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
    faces[i] = hand->card[i].face_val;
    // fprintf(stderr, "face_val: %d | ", faces[i]);
  }
  // putchar('\n');

  if (faces[0] == POKEVAL_ACE && faces[1] == DH_CARD_FIVE && faces[2] == DH_CARD_FOUR &&
      faces[3] == DH_CARD_THREE && faces[4] == DH_CARD_TWO) {
    // hand->card[0].face_val = DH_CARD_ACE;
    return true;
  }

  if (faces[0] == POKEVAL_ACE && faces[1] == DH_CARD_KING && faces[2] == DH_CARD_QUEEN &&
      faces[3] == DH_CARD_JACK && faces[4] == DH_CARD_TEN)
    return true;

  for (int i = 1; i < POKEVAL_HAND_SIZE; ++i)
    if (faces[i] != faces[i - 1] - 1)
      return false;

  return true;
}

short POKEVAL_evaluate_hand(POKEVAL_Hand_5 hand) {
  POKEVAL_sort_hand(&hand);

  bool flush = true;
  for (int i = 1; i < POKEVAL_HAND_SIZE; ++i)
    if (hand.card[i].suit != hand.card[0].suit)
      flush = false;

  bool straight = is_straight(&hand);

  // Royal flush is specifically A-K-Q-J-10 (not the A-2-3-4-5 wheel).
  if (straight && flush && hand.card[0].face_val == POKEVAL_ACE &&
      hand.card[1].face_val == DH_CARD_KING)
    return POKEVAL_ROYAL_FLUSH;
  if (straight && flush)
    return POKEVAL_STRAIGHT_FLUSH;
  if ((count_face(&hand, hand.card[0].face_val) == 5) ||
      (count_face(&hand, hand.card[4].face_val) == 5))
    return POKEVAL_FIVE_OF_A_KIND;
  if ((count_face(&hand, hand.card[0].face_val) == 4) ||
      (count_face(&hand, hand.card[4].face_val) == 4))
    return POKEVAL_FOUR_OF_A_KIND;
  if ((count_face(&hand, hand.card[0].face_val) == 3 &&
       count_face(&hand, hand.card[4].face_val) == 2) ||
      (count_face(&hand, hand.card[0].face_val) == 2 &&
       count_face(&hand, hand.card[4].face_val) == 3))
    return POKEVAL_FULL_HOUSE;
  if (flush)
    return POKEVAL_FLUSH;
  if (straight)
    return POKEVAL_STRAIGHT;
  if ((count_face(&hand, hand.card[0].face_val) == 3) ||
      (count_face(&hand, hand.card[2].face_val) == 3) ||
      (count_face(&hand, hand.card[4].face_val) == 3))
    return POKEVAL_THREE_OF_A_KIND;

  int pair_count = 0;
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i)
    if (count_face(&hand, hand.card[i].face_val) == 2)
      pair_count++;

  if (pair_count == 4)
    return POKEVAL_TWO_PAIR;
  if (pair_count == 2)
    return POKEVAL_PAIR;

  return POKEVAL_HIGH_CARD;
}

static int get_triplet_value(const POKEVAL_Hand_5 *hand) {
  for (int i = 0; i <= POKEVAL_HAND_SIZE - 3; ++i)
    if (count_face(hand, hand->card[i].face_val) == 3)
      return hand->card[i].face_val;
  return -1;
}

static int get_quint_value(const POKEVAL_Hand_5 *hand) {
  for (int i = 0; i <= POKEVAL_HAND_SIZE - 5; ++i)
    if (count_face(hand, hand->card[i].face_val) == 5)
      return hand->card[i].face_val;
  return -1;
}

static int get_quad_value(const POKEVAL_Hand_5 *hand) {
  for (int i = 0; i <= POKEVAL_HAND_SIZE - 4; ++i)
    if (count_face(hand, hand->card[i].face_val) == 4)
      return hand->card[i].face_val;
  return -1;
}

static int compare_high_cards(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b) {
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
    if (a->card[i].face_val > b->card[i].face_val)
      return 1;
    if (a->card[i].face_val < b->card[i].face_val)
      return -1;
  }
  return 0;
}

static int compare_one_pair_tiebreak(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b) {
  int a_pair = 0, b_pair = 0;
  int a_kickers[3] = {0}, b_kickers[3] = {0};
  int a_k = 0, b_k = 0;

  // Assumes hands are already sorted in descending order
  for (int i = 0; i < POKEVAL_HAND_SIZE - 1; ++i) {
    if (face_rank(a->card[i].face_val) == face_rank(a->card[i + 1].face_val)) {
      a_pair = face_rank(a->card[i].face_val);
      break;
    }
  }
  for (int i = 0; i < POKEVAL_HAND_SIZE - 1; ++i) {
    if (face_rank(b->card[i].face_val) == face_rank(b->card[i + 1].face_val)) {
      b_pair = face_rank(b->card[i].face_val);
      break;
    }
  }

  // Extract kickers
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
    int val_a = face_rank(a->card[i].face_val);
    int val_b = face_rank(b->card[i].face_val);

    if (val_a != a_pair && a_k < 3) {
      a_kickers[a_k++] = val_a;
    }
    if (val_b != b_pair && b_k < 3) {
      b_kickers[b_k++] = val_b;
    }
  }

  // Sort kickers descending (simple selection sort for 3 elements)
  for (int i = 0; i < 2; ++i) {
    for (int j = i + 1; j < 3; ++j) {
      if (a_kickers[j] > a_kickers[i]) {
        int tmp = a_kickers[i];
        a_kickers[i] = a_kickers[j];
        a_kickers[j] = tmp;
      }
      if (b_kickers[j] > b_kickers[i]) {
        int tmp = b_kickers[i];
        b_kickers[i] = b_kickers[j];
        b_kickers[j] = tmp;
      }
    }
  }

  // Compare pair
  if (a_pair > b_pair)
    return 1;
  if (a_pair < b_pair)
    return -1;

  // Compare kickers
  for (int i = 0; i < 3; ++i) {
    if (a_kickers[i] > b_kickers[i])
      return 1;
    if (a_kickers[i] < b_kickers[i])
      return -1;
  }

  return 0; // hands are tied
}

static int compare_two_pair_tiebreak(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b) {
  int a_high = 0, a_low = 0, a_kicker = 0;
  int b_high = 0, b_low = 0, b_kicker = 0;

  // Assumes hand is sorted high to low
  for (int i = 0; i < POKEVAL_HAND_SIZE - 1; ++i) {
    if (a->card[i].face_val == a->card[i + 1].face_val) {
      if (a_high == 0)
        a_high = a->card[i].face_val;
      else if (a_low == 0 && a->card[i].face_val != a_high)
        a_low = a->card[i].face_val;
    }
    if (b->card[i].face_val == b->card[i + 1].face_val) {
      if (b_high == 0)
        b_high = b->card[i].face_val;
      else if (b_low == 0 && b->card[i].face_val != b_high)
        b_low = b->card[i].face_val;
    }
  }

  if (a_high < a_low) {
    int tmp = a_high;
    a_high = a_low;
    a_low = tmp;
  }
  if (b_high < b_low) {
    int tmp = b_high;
    b_high = b_low;
    b_low = tmp;
  }

  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
    int val = a->card[i].face_val;
    if (val != a_high && val != a_low)
      a_kicker = val;
    val = b->card[i].face_val;
    if (val != b_high && val != b_low)
      b_kicker = val;
  }

  if (a_high > b_high)
    return -1;
  if (a_high < b_high)
    return 1;
  if (a_low > b_low)
    return -1;
  if (a_low < b_low)
    return 1;
  if (a_kicker > b_kicker)
    return -1;
  if (a_kicker < b_kicker)
    return 1;
  return 0;
}

static int get_kicker_value(const POKEVAL_Hand_5 *hand, int quad_val) {
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
    if (hand->card[i].face_val != quad_val)
      return hand->card[i].face_val;
  }
  return -1; // Should never happen in valid hand
}

static uint8_t compare_hands_5(POKEVAL_NeedComparing *need_comparing, uint8_t count) {
  for (size_t i = 0; i < count; ++i) {
    POKEVAL_sort_hand(&need_comparing[i].hand_5);
  }

  uint8_t num_winners = 0;
  short best_rank = -1;
  POKEVAL_Hand_5 best_hand = {0};
  assert(count > 0);
  uint8_t winner_indices[UINT8_MAX];
  winner_indices[0] = 0;

  // Evaluate all hands and determine the best one(s)
  for (uint8_t i = 0; i < count; ++i) {
    need_comparing[i].won = false;
    POKEVAL_Hand_5 current_hand = need_comparing[i].hand_5;
    short rank = POKEVAL_evaluate_hand(current_hand);

    if (rank > best_rank) {
      best_rank = rank;
      best_hand = current_hand;
      winner_indices[0] = i;
      num_winners = 1;
    } else if (rank == best_rank) {
      POKEVAL_Hand_5 a = best_hand;
      POKEVAL_Hand_5 b = current_hand;
      // sort_hand(&a);
      // sort_hand(&b);

      bool b_wins = false;
      bool tie = false;

      switch (rank) {
      case POKEVAL_ROYAL_FLUSH:
        tie = true;
        break;
      case POKEVAL_STRAIGHT_FLUSH:
      case POKEVAL_STRAIGHT: {
        int a_high = a.card[0].face_val;
        int b_high = b.card[0].face_val;
        if (a_high == POKEVAL_ACE && a.card[1].face_val == DH_CARD_FIVE)
          a_high = 5;
        if (b_high == POKEVAL_ACE && b.card[1].face_val == DH_CARD_FIVE)
          b_high = 5;
        if (a_high == b_high)
          tie = true;
        else
          b_wins = b_high > a_high;
        break;
      }
      case POKEVAL_FIVE_OF_A_KIND: {
        int a_quint = get_quint_value(&a);
        int b_quint = get_quint_value(&b);
        if (a_quint == b_quint)
          tie = true;
        else
          b_wins = b_quint > a_quint;
        break;
      }
      case POKEVAL_FOUR_OF_A_KIND: {
        int a_quad = get_quad_value(&a);
        int b_quad = get_quad_value(&b);

        int a_kicker = get_kicker_value(&a, a_quad);
        int b_kicker = get_kicker_value(&b, b_quad);

        if (a_quad == b_quad) {
          if (a_kicker == b_kicker)
            tie = true;
          else
            b_wins = b_kicker > a_kicker;
        } else {
          b_wins = b_quad > a_quad;
        }
        break;
      }
      case POKEVAL_FULL_HOUSE: {
        int a_trip = get_triplet_value(&a);
        int b_trip = get_triplet_value(&b);
        int a_pair = -1, b_pair = -1;
        for (int j = 0; j < POKEVAL_HAND_SIZE; ++j) {
          int val = a.card[j].face_val;
          if (val != a_trip && count_face(&a, val) == 2) {
            a_pair = val;
            break;
          }
        }
        for (int j = 0; j < POKEVAL_HAND_SIZE; ++j) {
          int val = b.card[j].face_val;
          if (val != b_trip && count_face(&b, val) == 2) {
            b_pair = val;
            break;
          }
        }
        if (a_trip == b_trip && a_pair == b_pair)
          tie = true;
        else if (b_trip > a_trip || (b_trip == a_trip && b_pair > a_pair))
          b_wins = true;
        break;
      }
      case POKEVAL_FLUSH:
      case POKEVAL_HIGH_CARD: {
        int cmp = compare_high_cards(&a, &b);
        if (cmp == 0)
          tie = true;
        else if (cmp < 0)
          b_wins = true;
        break;
      }
      case POKEVAL_THREE_OF_A_KIND: {
        int a_trip = get_triplet_value(&a);
        int b_trip = get_triplet_value(&b);
        if (a_trip != b_trip) {
          b_wins = b_trip > a_trip;
        } else {
          int cmp = compare_high_cards(&a, &b);
          if (cmp == 0)
            tie = true;
          else if (cmp < 0)
            b_wins = true;
        }
        break;
      }
      case POKEVAL_TWO_PAIR: {
        int cmp = compare_two_pair_tiebreak(&a, &b);
        if (cmp == 0)
          tie = true;
        else if (cmp > 0)
          b_wins = true;
        break;
      }
      case POKEVAL_PAIR: {
        int cmp = compare_one_pair_tiebreak(&a, &b);
        if (cmp == 0)
          tie = true;
        else if (cmp < 0)
          b_wins = true;
        break;
      }
      }

      if (tie) {
        winner_indices[num_winners++] = i;
      } else if (b_wins) {
        best_hand = b;
        winner_indices[0] = i;
        num_winners = 1;
      }
    }
  }

  // Mark only the winners
  for (uint8_t i = 0; i < num_winners; ++i) {
    need_comparing[winner_indices[i]].won = true;
  }

  return num_winners;
}

POKEVAL_Hand_5 POKEVAL_hand5_from_hand7(const POKEVAL_Hand_9 *src) {
  // Count valid (non-null) cards
  size_t n = 0;
  while (n < 9 && !DH_is_card_null(src->card[n]))
    n++;

  // Fast path: already a 5-card hand
  if (n <= 5) {
    POKEVAL_Hand_5 dest = {0};
    for (size_t i = 0; i < n; ++i)
      dest.card[i] = src->card[i];
    return dest;
  }

  POKEVAL_Hand_5 best_hand = {0};
  short best_rank = -1;
  DH_Card temp[5];

  if (n == 6) {
    // 6-card hand: try all C(6,1)=6 candidates by omitting one card
    for (size_t i = 0; i < 6; ++i) {
      size_t k = 0;
      for (size_t m = 0; m < 6; ++m) {
        if (m != i)
          temp[k++] = src->card[m];
      }

      POKEVAL_Hand_5 candidate = {0};
      memcpy(candidate.card, temp, sizeof(temp));

      short rank = POKEVAL_evaluate_hand(candidate);
      POKEVAL_sort_hand(&candidate);

      if (rank > best_rank) {
        best_rank = rank;
        best_hand = candidate;
      } else if (rank == best_rank) {
        bool candidate_better;
        if (rank == POKEVAL_STRAIGHT || rank == POKEVAL_STRAIGHT_FLUSH) {
          int cand_high = candidate.card[0].face_val;
          int best_high = best_hand.card[0].face_val;
          if (cand_high == POKEVAL_ACE && candidate.card[1].face_val == DH_CARD_FIVE)
            cand_high = DH_CARD_FIVE;
          if (best_high == POKEVAL_ACE && best_hand.card[1].face_val == DH_CARD_FIVE)
            best_high = DH_CARD_FIVE;
          candidate_better = cand_high > best_high;
        } else {
          candidate_better = compare_high_cards(&candidate, &best_hand) > 0;
        }
        if (candidate_better)
          best_hand = candidate;
      }
    }
    return best_hand;
  }

  // Full 7-card evaluation: try all C(7,2)=21 candidates by omitting two cards
  for (size_t i = 0; i < 7; ++i) {
    for (size_t j = i + 1; j < 7; ++j) {
      // Build candidate hand by omitting cards i and j
      size_t k = 0;
      for (size_t m = 0; m < 7; ++m) {
        if (m != i && m != j) {
          temp[k++] = src->card[m];
        }
      }

      POKEVAL_Hand_5 candidate = {0};
      memcpy(candidate.card, temp, sizeof(temp));

      short rank = POKEVAL_evaluate_hand(candidate);
      POKEVAL_sort_hand(&candidate);

      if (rank > best_rank) {
        best_rank = rank;
        best_hand = candidate;
      } else if (rank == best_rank) {
        bool candidate_better;
        if (rank == POKEVAL_STRAIGHT || rank == POKEVAL_STRAIGHT_FLUSH) {
          int cand_high = candidate.card[0].face_val;
          int best_high = best_hand.card[0].face_val;
          if (cand_high == POKEVAL_ACE && candidate.card[1].face_val == DH_CARD_FIVE)
            cand_high = DH_CARD_FIVE;
          if (best_high == POKEVAL_ACE && best_hand.card[1].face_val == DH_CARD_FIVE)
            best_high = DH_CARD_FIVE;
          candidate_better = cand_high > best_high;
        } else {
          candidate_better = compare_high_cards(&candidate, &best_hand) > 0;
        }
        if (candidate_better)
          best_hand = candidate;
      }
    }
  }

  return best_hand;
}

static inline int lowball_value(int face) { return (face == DH_CARD_ACE) ? 1 : face; }

void POKEVAL_sort_hand_lowball(POKEVAL_Hand_5 *hand) {
  for (int i = 0; i < POKEVAL_HAND_SIZE - 1; ++i) {
    for (int j = i + 1; j < POKEVAL_HAND_SIZE; ++j) {

      int val_i = lowball_value(hand->card[i].face_val);
      int val_j = lowball_value(hand->card[j].face_val);

      // ascending (low wins)
      if (val_i > val_j) {
        DH_Card tmp = hand->card[i];
        hand->card[i] = hand->card[j];
        hand->card[j] = tmp;
      }
    }
  }
}

// Flatten a hand into ace-to-five compare order: card groups sorted by count
// descending, then value descending (ace = 1), each value repeated count
// times. Comparing two keys element-wise with lower-wins is the inverse of
// the high-hand ranking within a class — e.g. for one pair, the pair value
// decides before the kickers.
static void lowball_key(const int counts[15], int key[5]) {
  int k = 0;
  for (int c = 4; c >= 1; --c)
    for (int v = 14; v >= 1; --v)
      if (counts[v] == c)
        for (int i = 0; i < c; ++i)
          key[k++] = v;
}

static int compare_lowball_5(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b) {
  // assumes both hands are sorted low → high with Ace = 1

  int a_counts[15] = {0};
  int b_counts[15] = {0};

  for (int i = 0; i < 5; ++i) {
    a_counts[a->card[i].face_val]++;
    b_counts[b->card[i].face_val]++;
  }

  // 1) classify by the two largest duplicate counts, worst to best:
  //    (4,1) quads, (3,2) full house, (3,1) trips, (2,2) two pair,
  //    (2,1) one pair, (1,1) no pair — lexicographically lower is better
  int a_max = 0, a_2nd = 0, b_max = 0, b_2nd = 0;
  for (int v = 1; v <= 14; ++v) {
    if (a_counts[v] >= a_max) {
      a_2nd = a_max;
      a_max = a_counts[v];
    } else if (a_counts[v] > a_2nd) {
      a_2nd = a_counts[v];
    }
    if (b_counts[v] >= b_max) {
      b_2nd = b_max;
      b_max = b_counts[v];
    } else if (b_counts[v] > b_2nd) {
      b_2nd = b_counts[v];
    }
  }

  if (a_max != b_max)
    return (a_max < b_max) ? -1 : 1;
  if (a_2nd != b_2nd)
    return (a_2nd < b_2nd) ? -1 : 1;

  // 2) same class → compare grouped values (low wins)
  int a_key[5], b_key[5];
  lowball_key(a_counts, a_key);
  lowball_key(b_counts, b_key);
  for (int i = 0; i < 5; ++i) {
    if (a_key[i] != b_key[i])
      return (a_key[i] < b_key[i]) ? -1 : 1;
  }

  return 0; // tie
}

static uint8_t compare_hands_5_lowball(POKEVAL_NeedComparing *need_comparing, uint8_t count) {
  for (uint8_t i = 0; i < count; ++i) {
    POKEVAL_sort_hand_lowball(&need_comparing[i].hand_5);
    need_comparing[i].won = false;
  }

  assert(count > 0);
  uint8_t winner_indices[UINT8_MAX];
  winner_indices[0] = 0;
  uint8_t num_winners = 0;

  POKEVAL_Hand_5 best_hand = need_comparing[0].hand_5;
  winner_indices[0] = 0;
  num_winners = 1;

  for (uint8_t i = 1; i < count; ++i) {
    int cmp = compare_lowball_5(&best_hand, &need_comparing[i].hand_5);

    if (cmp == 0) {
      winner_indices[num_winners++] = i;
    } else if (cmp > 0) {
      best_hand = need_comparing[i].hand_5;
      winner_indices[0] = i;
      num_winners = 1;
    }
  }

  for (uint8_t i = 0; i < num_winners; ++i) {
    need_comparing[winner_indices[i]].won = true;
  }

  return num_winners;
}

uint8_t POKEVAL_compare_hands(POKEVAL_NeedComparing *need_comparing, uint8_t count,
                              const bool lowball) {

  for (size_t i = 0; i < count; ++i) {
    need_comparing[i].hand_5 = POKEVAL_hand5_from_hand7(&need_comparing[i].hand);
  }
  return lowball == false ? compare_hands_5(need_comparing, count)
                          : compare_hands_5_lowball(need_comparing, count);
}

// Wild card evaluation -------------------------------------------------------

static const int32_t wild_face_values[13] = {
    DH_CARD_ACE,  DH_CARD_TWO,   DH_CARD_THREE, DH_CARD_FOUR, DH_CARD_FIVE,
    DH_CARD_SIX,  DH_CARD_SEVEN, DH_CARD_EIGHT, DH_CARD_NINE, DH_CARD_TEN,
    DH_CARD_JACK, DH_CARD_QUEEN, DH_CARD_KING,
};
#define WILD_FACE_COUNT 13

// Recursively try all face value assignments for wilds; returns the best rank.
// Each wild is assigned `suit` as its suit (caller passes flush or non-flush suit).
static short try_wild_combos(DH_Card *real_cards, int n_real, int n_wild, int wild_idx,
                             DH_Card *current_wilds, int32_t suit) {
  if (wild_idx == n_wild) {
    POKEVAL_Hand_5 hand = {0};
    for (int i = 0; i < n_real; i++)
      hand.card[i] = real_cards[i];
    for (int i = 0; i < n_wild; i++)
      hand.card[n_real + i] = current_wilds[i];
    return POKEVAL_evaluate_hand(hand);
  }

  short best = POKEVAL_HIGH_CARD;
  for (int fi = 0; fi < WILD_FACE_COUNT; fi++) {
    current_wilds[wild_idx].face_val = wild_face_values[fi];
    current_wilds[wild_idx].suit = suit;
    short rank = try_wild_combos(real_cards, n_real, n_wild, wild_idx + 1, current_wilds, suit);
    if (rank > best) {
      best = rank;
      if (best == POKEVAL_FIVE_OF_A_KIND)
        return best;
    }
  }
  return best;
}

short POKEVAL_evaluate_hand_wild(POKEVAL_Hand_5 hand, int32_t wild_face) {
  DH_Card real_cards[POKEVAL_HAND_SIZE];
  int n_real = 0, n_wild = 0;

  for (int i = 0; i < POKEVAL_HAND_SIZE; i++) {
    if (hand.card[i].face_val == wild_face)
      n_wild++;
    else
      real_cards[n_real++] = hand.card[i];
  }

  if (n_wild == 0)
    return POKEVAL_evaluate_hand(hand);

  if (n_real == 0)
    return POKEVAL_FIVE_OF_A_KIND; // all wilds → five aces

  // Determine the dominant suit among real cards for flush checking.
  int suit_count[4] = {0};
  for (int i = 0; i < n_real; i++) {
    int s = real_cards[i].suit;
    if (s >= 0 && s < 4)
      suit_count[s]++;
  }
  int dom_suit = 0;
  for (int s = 1; s < 4; s++)
    if (suit_count[s] > suit_count[dom_suit])
      dom_suit = s;

  bool all_same_suit = (suit_count[dom_suit] == n_real);
  // Non-flush: assign wilds a different suit so flush detection stays false.
  int32_t non_flush_suit = (int32_t)((dom_suit + 1) % 4);

  DH_Card wilds[4] = {0};

  short best = try_wild_combos(real_cards, n_real, n_wild, 0, wilds, non_flush_suit);

  // Flush check: only possible when all real cards share a suit.
  if (all_same_suit && best < POKEVAL_FIVE_OF_A_KIND) {
    short flush_best = try_wild_combos(real_cards, n_real, n_wild, 0, wilds, (int32_t)dom_suit);
    if (flush_best > best)
      best = flush_best;
  }

  return best;
}

// Forward declarations for the wild-aware tie-break helpers that
// compare_wild_same_rank dispatches to (all defined further down).
static int get_group_value_wild(const POKEVAL_Hand_5 *hand, int32_t wild_face, int group_size);
static int compare_pair_tiebreak_wild(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b,
                                      int32_t wild_face, int n_pairs);
static int compare_kind_tiebreak_wild(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b,
                                      int32_t wild_face, int group_size);
static int straight_high_wild(const POKEVAL_Hand_5 *hand, int32_t wild_face);
static int compare_flush_tiebreak_wild(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b,
                                       int32_t wild_face);

// Compare two equal-rank wild-aware hands.  Returns >0 if a is better,
// <0 if b is better, 0 if tied.  Mirrors the switch inside
// POKEVAL_compare_hands_wild but factored out so update_best_wild and
// the main winner-selection share the same wild-substituted tie-break
// logic — otherwise the best-5-of-7 picker rejects a wild-aided K-high
// straight in favour of an inferior natural Q-high one because
// compare_high_cards sees the wild as a literal 2.
static int compare_wild_same_rank(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b, short rank,
                                  int32_t wild_face) {
  POKEVAL_Hand_5 as = *a, bs = *b;
  POKEVAL_sort_hand(&as);
  POKEVAL_sort_hand(&bs);

  switch (rank) {
  case POKEVAL_ROYAL_FLUSH:
    return 0;
  case POKEVAL_FIVE_OF_A_KIND: {
    int av = get_group_value_wild(&as, wild_face, 5);
    int bv = get_group_value_wild(&bs, wild_face, 5);
    if (av != bv)
      return av - bv;
    return 0;
  }
  case POKEVAL_FOUR_OF_A_KIND:
    return compare_kind_tiebreak_wild(&as, &bs, wild_face, 4);
  case POKEVAL_FULL_HOUSE: {
    int at = get_group_value_wild(&as, wild_face, 3);
    int bt = get_group_value_wild(&bs, wild_face, 3);
    if (at != bt)
      return at - bt;
    int ap = -1, bp = -1;
    for (int j = 0; j < POKEVAL_HAND_SIZE; j++) {
      int val = as.card[j].face_val;
      if (val != at && val != wild_face && count_face(&as, val) == 2) {
        ap = val;
        break;
      }
    }
    for (int j = 0; j < POKEVAL_HAND_SIZE; j++) {
      int val = bs.card[j].face_val;
      if (val != bt && val != wild_face && count_face(&bs, val) == 2) {
        bp = val;
        break;
      }
    }
    return ap - bp;
  }
  case POKEVAL_THREE_OF_A_KIND:
    return compare_kind_tiebreak_wild(&as, &bs, wild_face, 3);
  case POKEVAL_TWO_PAIR:
    return compare_pair_tiebreak_wild(&as, &bs, wild_face, 2);
  case POKEVAL_PAIR:
    return compare_pair_tiebreak_wild(&as, &bs, wild_face, 1);
  case POKEVAL_STRAIGHT:
  case POKEVAL_STRAIGHT_FLUSH: {
    int ah = straight_high_wild(&as, wild_face);
    int bh = straight_high_wild(&bs, wild_face);
    return ah - bh;
  }
  case POKEVAL_FLUSH:
    return compare_flush_tiebreak_wild(&as, &bs, wild_face);
  default:
    return compare_high_cards(&as, &bs);
  }
}

static void update_best_wild(POKEVAL_Hand_5 *best_hand, short *best_rank, POKEVAL_Hand_5 candidate,
                             int32_t wild_face) {
  short rank = POKEVAL_evaluate_hand_wild(candidate, wild_face);
  if (rank > *best_rank) {
    *best_rank = rank;
    *best_hand = candidate;
  } else if (rank == *best_rank) {
    if (compare_wild_same_rank(&candidate, best_hand, rank, wild_face) > 0)
      *best_hand = candidate;
  }
}

POKEVAL_Hand_5 POKEVAL_hand5_from_hand7_wild(const POKEVAL_Hand_9 *src, int32_t wild_face) {
  size_t n = 0;
  while (n < 9 && !DH_is_card_null(src->card[n]))
    n++;

  if (n <= 5) {
    POKEVAL_Hand_5 dest = {0};
    for (size_t i = 0; i < n; i++)
      dest.card[i] = src->card[i];
    return dest;
  }

  POKEVAL_Hand_5 best_hand = {0};
  short best_rank = -1;
  DH_Card temp[5];

  if (n == 6) {
    // C(6,5) = 6: omit one card at a time
    for (size_t i = 0; i < 6; i++) {
      size_t k = 0;
      for (size_t m = 0; m < 6; m++)
        if (m != i)
          temp[k++] = src->card[m];
      POKEVAL_Hand_5 candidate = {0};
      memcpy(candidate.card, temp, sizeof(temp));
      update_best_wild(&best_hand, &best_rank, candidate, wild_face);
    }
    return best_hand;
  }

  // C(7,5) = 21: omit two cards at a time
  for (size_t i = 0; i < 7; i++) {
    for (size_t j = i + 1; j < 7; j++) {
      size_t k = 0;
      for (size_t m = 0; m < 7; m++)
        if (m != i && m != j)
          temp[k++] = src->card[m];
      POKEVAL_Hand_5 candidate = {0};
      memcpy(candidate.card, temp, sizeof(temp));
      update_best_wild(&best_hand, &best_rank, candidate, wild_face);
    }
  }

  return best_hand;
}

// Helper: update best 5-card hand with proper tie-breaking for all ranks.
// Without rank-aware tie-breaks, compare_high_cards walks sorted positions
// and picks the wrong winner whenever a high kicker outranks the pair
// value's position — e.g. two-pair 7+6 with K kicker (sorted K,7,7,6,6)
// would beat two-pair J+7 with Q kicker (sorted Q,J,J,7,7) at position 0
// since K > Q, even though J+7 is the stronger two pair.  Caught in
// pokeval_fuzz running across Omaha (where one player has multiple
// equal-rank candidate hands and we must pick the genuinely best one).
static void update_best_5card(POKEVAL_Hand_5 *best_hand, short *best_rank, POKEVAL_Hand_5 candidate,
                              short rank) {
  if (rank > *best_rank) {
    *best_rank = rank;
    *best_hand = candidate;
    return;
  }
  if (rank != *best_rank)
    return;

  bool candidate_better = false;
  switch (rank) {
  case POKEVAL_STRAIGHT:
  case POKEVAL_STRAIGHT_FLUSH: {
    int cand_high = candidate.card[0].face_val;
    int best_high = best_hand->card[0].face_val;
    if (cand_high == POKEVAL_ACE && candidate.card[1].face_val == DH_CARD_FIVE)
      cand_high = DH_CARD_FIVE;
    if (best_high == POKEVAL_ACE && best_hand->card[1].face_val == DH_CARD_FIVE)
      best_high = DH_CARD_FIVE;
    candidate_better = cand_high > best_high;
    break;
  }
  case POKEVAL_PAIR:
    candidate_better = compare_one_pair_tiebreak(&candidate, best_hand) > 0;
    break;
  case POKEVAL_TWO_PAIR:
    /* compare_two_pair_tiebreak's inverted convention: returns -1 if a
     * (candidate) is better. */
    candidate_better = compare_two_pair_tiebreak(&candidate, best_hand) < 0;
    break;
  default:
    /* FLUSH and HIGH_CARD both rank by raw high cards.
     * THREE_OF_A_KIND / FOUR / FULL_HOUSE / FIVE: pokeval's sort puts
     * the multi-card group at the top, so compare_high_cards is correct
     * for those too. */
    candidate_better = compare_high_cards(&candidate, best_hand) > 0;
    break;
  }
  if (candidate_better)
    *best_hand = candidate;
}

POKEVAL_Hand_5 POKEVAL_hand5_omaha(const POKEVAL_Hand_9 *src) {
  // Hole cards: positions 0-3; community cards: positions 4-8.
  // Must use exactly 2 hole cards and 3 community cards (C(4,2) x C(5,3) = 60 combos).
  const DH_Card *hole = src->card;
  const DH_Card *comm = src->card + 4;

  POKEVAL_Hand_5 best_hand = {0};
  short best_rank = -1;

  for (int h1 = 0; h1 < 4; h1++) {
    for (int h2 = h1 + 1; h2 < 4; h2++) {
      for (int c1 = 0; c1 < 5; c1++) {
        for (int c2 = c1 + 1; c2 < 5; c2++) {
          for (int c3 = c2 + 1; c3 < 5; c3++) {
            POKEVAL_Hand_5 candidate = {0};
            candidate.card[0] = hole[h1];
            candidate.card[1] = hole[h2];
            candidate.card[2] = comm[c1];
            candidate.card[3] = comm[c2];
            candidate.card[4] = comm[c3];
            short rank = POKEVAL_evaluate_hand(candidate);
            POKEVAL_sort_hand(&candidate);
            update_best_5card(&best_hand, &best_rank, candidate, rank);
          }
        }
      }
    }
  }
  return best_hand;
}

uint8_t POKEVAL_compare_hands_omaha(POKEVAL_NeedComparing *need_comparing, uint8_t count) {
  for (size_t i = 0; i < count; ++i)
    need_comparing[i].hand_5 = POKEVAL_hand5_omaha(&need_comparing[i].hand);
  return compare_hands_5(need_comparing, count);
}

// Returns the face value of the highest N-of-a-kind group in a wild hand.
// Checks for a natural group of exactly `group_size` first, then falls back
// to the highest non-wild face whose natural count plus available wilds
// reaches `group_size`.
static int get_group_value_wild(const POKEVAL_Hand_5 *hand, int32_t wild_face, int group_size) {
  for (int i = 0; i < POKEVAL_HAND_SIZE; i++) {
    int fv = hand->card[i].face_val;
    if (fv != wild_face && count_face(hand, fv) == group_size)
      return fv;
  }
  int n_wild = 0;
  for (int i = 0; i < POKEVAL_HAND_SIZE; i++)
    if (hand->card[i].face_val == wild_face)
      n_wild++;
  int best = -1;
  for (int i = 0; i < POKEVAL_HAND_SIZE; i++) {
    int fv = hand->card[i].face_val;
    if (fv == wild_face)
      continue;
    if (count_face(hand, fv) + n_wild >= group_size && fv > best)
      best = fv;
  }
  return best;
}

uint8_t POKEVAL_compare_hands_wild(POKEVAL_NeedComparing *need_comparing, uint8_t count,
                                   int32_t wild_face) {
  for (size_t i = 0; i < count; ++i)
    need_comparing[i].hand_5 = POKEVAL_hand5_from_hand7_wild(&need_comparing[i].hand, wild_face);

  uint8_t num_winners = 0;
  short best_rank = -1;
  POKEVAL_Hand_5 best_hand = {0};
  assert(count > 0);
  uint8_t winner_indices[UINT8_MAX];
  winner_indices[0] = 0;

  for (uint8_t i = 0; i < count; ++i) {
    need_comparing[i].won = false;
    POKEVAL_Hand_5 current_hand = need_comparing[i].hand_5;
    short rank = POKEVAL_evaluate_hand_wild(current_hand, wild_face);

    if (rank > best_rank) {
      best_rank = rank;
      best_hand = current_hand;
      winner_indices[0] = i;
      num_winners = 1;
    } else if (rank == best_rank) {
      int cmp = compare_wild_same_rank(&best_hand, &current_hand, rank, wild_face);
      if (cmp == 0) {
        winner_indices[num_winners++] = i;
      } else if (cmp < 0) {
        best_hand = current_hand;
        winner_indices[0] = i;
        num_winners = 1;
      }
    }
  }

  for (uint8_t i = 0; i < num_winners; ++i)
    need_comparing[winner_indices[i]].won = true;

  return num_winners;
}

// Substitute wilds into their pair role(s) and defer to the natural
// pair/two-pair tie-break.  Without this, the default (compare_high_cards)
// comparison just walks the sorted hand position-by-position — for a
// natural pair 5s with K-T kickers vs natural pair 8s with Q-J kickers,
// both sort with the high non-pair card first (K vs Q), and the kicker
// comparison fires before the pair value ever gets compared.
//
// `n_pairs` is 1 (one-pair) or 2 (two-pair); the wild evaluator already
// picked the category for us.
static int compare_pair_tiebreak_wild(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b,
                                      int32_t wild_face, int n_pairs) {
  POKEVAL_Hand_5 as = *a, bs = *b;

  for (int which = 0; which < 2; which++) {
    POKEVAL_Hand_5 *h = (which == 0) ? &as : &bs;

    // Top pair: get_group_value_wild returns the highest face whose
    // natural count + wilds reaches 2.  Substitute wilds into it until
    // we have two cards of that face.
    int top = get_group_value_wild(h, wild_face, 2);
    if (top > 0) {
      int natural_top = count_face(h, top);
      int wilds_needed_top = (natural_top >= 2) ? 0 : (2 - natural_top);
      for (int i = 0; i < POKEVAL_HAND_SIZE && wilds_needed_top > 0; i++) {
        if (h->card[i].face_val == wild_face) {
          h->card[i].face_val = top;
          wilds_needed_top--;
        }
      }
    }

    if (n_pairs == 2) {
      // Second pair: look for the next-highest face whose natural count
      // (no further wilds available — we may have already consumed them)
      // is 2, or count + remaining wilds reaches 2.
      int second = -1;
      for (int fv = DH_CARD_KING + 1; fv >= DH_CARD_ACE; fv--) {
        if (fv == wild_face || fv == top)
          continue;
        // POKEVAL_ACE == DH_CARD_KING + 1 is the iteration start above;
        // skip values that aren't dealt as themselves.
        if (fv == POKEVAL_ACE && fv != DH_CARD_ACE)
          continue;
        int cnt = count_face(h, fv);
        int wilds_left = count_face(h, wild_face);
        if (cnt + wilds_left >= 2 && fv > second)
          second = fv;
      }
      if (second > 0) {
        int natural_second = count_face(h, second);
        int wilds_needed_second = (natural_second >= 2) ? 0 : (2 - natural_second);
        for (int i = 0; i < POKEVAL_HAND_SIZE && wilds_needed_second > 0; i++) {
          if (h->card[i].face_val == wild_face) {
            h->card[i].face_val = second;
            wilds_needed_second--;
          }
        }
      }
    }

    // Any remaining wild becomes ace-high (the best kicker).
    for (int i = 0; i < POKEVAL_HAND_SIZE; i++)
      if (h->card[i].face_val == wild_face)
        h->card[i].face_val = POKEVAL_ACE;

    POKEVAL_sort_hand(h);
  }

  /* compare_one_pair_tiebreak and compare_two_pair_tiebreak use INVERTED
   * return conventions (one-pair: >0 = a wins; two-pair: <0 = a wins).
   * Normalize to one-pair convention here so the caller can use a single
   * "<0 means b wins" check for both ranks. */
  if (n_pairs == 1)
    return compare_one_pair_tiebreak(&as, &bs);
  return -compare_two_pair_tiebreak(&as, &bs);
}

// Wild-aware tie-break for N-of-a-kind hands (group_size = 3 or 4).
// Substitutes wilds into the group role so the natural compare_high_cards
// can read kickers correctly.  Without this, compare_high_cards walks the
// raw sorted hand and treats a natural duplicate (e.g. a second natural
// ace in a hand with trip aces) as a kicker, vs. a wild card sitting at
// face_val=2 in the opponent's hand — and picks the wrong winner.
static int compare_kind_tiebreak_wild(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b,
                                      int32_t wild_face, int group_size) {
  int a_val = get_group_value_wild(a, wild_face, group_size);
  int b_val = get_group_value_wild(b, wild_face, group_size);
  if (a_val != b_val)
    return a_val - b_val;

  POKEVAL_Hand_5 as = *a, bs = *b;
  for (int which = 0; which < 2; which++) {
    POKEVAL_Hand_5 *h = (which == 0) ? &as : &bs;
    int val = (which == 0) ? a_val : b_val;
    int natural = count_face(h, val);
    int wilds_needed = (natural >= group_size) ? 0 : (group_size - natural);
    for (int i = 0; i < POKEVAL_HAND_SIZE && wilds_needed > 0; i++) {
      if (h->card[i].face_val == wild_face) {
        h->card[i].face_val = val;
        wilds_needed--;
      }
    }
    /* Any wild left over plays as ACE_HIGH (the best kicker). */
    for (int i = 0; i < POKEVAL_HAND_SIZE; i++)
      if (h->card[i].face_val == wild_face)
        h->card[i].face_val = POKEVAL_ACE;
    POKEVAL_sort_hand(h);
  }
  return compare_high_cards(&as, &bs);
}

// Returns the highest straight value the hand can reach using available
// wilds.  POKEVAL_ACE (14) is the high end; DH_CARD_FIVE is the low end
// (covers the A-2-3-4-5 wheel).  0 if no straight is possible.
//
// The caller has already established (via POKEVAL_evaluate_hand_wild)
// that the hand IS a straight at the same rank as the comparand; this
// just figures out *which* straight.
static int straight_high_wild(const POKEVAL_Hand_5 *hand, int32_t wild_face) {
  bool present[15] = {false}; /* index 1..14, with 14 == POKEVAL_ACE */
  int n_wild = 0;
  for (int i = 0; i < POKEVAL_HAND_SIZE; i++) {
    int fv = hand->card[i].face_val;
    if (fv == wild_face) {
      n_wild++;
      continue;
    }
    if (fv == DH_CARD_ACE || fv == POKEVAL_ACE) {
      present[1] = true;
      present[POKEVAL_ACE] = true;
    } else if (fv >= DH_CARD_TWO && fv <= DH_CARD_KING) {
      present[fv] = true;
    }
  }
  /* Highest straight first (broadway), down to the wheel. */
  for (int high = POKEVAL_ACE; high >= DH_CARD_FIVE; high--) {
    int hits = 0;
    for (int k = 0; k < 5; k++)
      if (present[high - k])
        hits++;
    if (hits + n_wild >= 5)
      return high;
  }
  return 0;
}

// Substitute each wild with ACE_HIGH and defer to compare_high_cards.
// This matches try_wild_combos's flush evaluation, which iterates every
// face and picks the highest-ranking POKEVAL_evaluate_hand result; for
// FLUSH that's always wild=ACE (since the substituted hand still ranks
// as FLUSH and the duplicate-ace reading dominates compare_high_cards).
//
// Greedy unique substitution would pick a lower face (e.g. K when ace
// is already in the hand) and produce a strictly weaker flush than the
// one try_wild_combos picked when computing the rank, leaving the two
// internally inconsistent.
static int compare_flush_tiebreak_wild(const POKEVAL_Hand_5 *a, const POKEVAL_Hand_5 *b,
                                       int32_t wild_face) {
  POKEVAL_Hand_5 as = *a, bs = *b;
  for (int which = 0; which < 2; which++) {
    POKEVAL_Hand_5 *h = (which == 0) ? &as : &bs;
    for (int i = 0; i < POKEVAL_HAND_SIZE; i++)
      if (h->card[i].face_val == wild_face)
        h->card[i].face_val = POKEVAL_ACE;
    POKEVAL_sort_hand(h);
  }
  return compare_high_cards(&as, &bs);
}

// Bring-in helpers -----------------------------------------------------------

int POKEVAL_suit_bringin_rank(int32_t suit) {
  switch (suit) {
  case DH_SUIT_CLUBS:
    return 0;
  case DH_SUIT_DIAMONDS:
    return 1;
  case DH_SUIT_HEARTS:
    return 2;
  case DH_SUIT_SPADES:
    return 3;
  default:
    return -1;
  }
}

bool POKEVAL_card_bringin_lt(DH_Card a, DH_Card b) {
  int ra = face_rank(a.face_val);
  int rb = face_rank(b.face_val);
  if (ra != rb)
    return ra < rb;
  return POKEVAL_suit_bringin_rank(a.suit) < POKEVAL_suit_bringin_rank(b.suit);
}

uint64_t POKEVAL_score_stud_upcards(const DH_Card *cards, int n) {
  if (n <= 0)
    return 0;

  // Collect face values (ace-high) and track suit of the highest card for tiebreaking.
  int faces[4] = {0};
  int best_suit = 0, best_face_val = 0;
  for (int i = 0; i < n && i < 4; i++) {
    int fv = face_rank(cards[i].face_val);
    faces[i] = fv;
    if (fv > best_face_val) {
      best_face_val = fv;
      best_suit = POKEVAL_suit_bringin_rank(cards[i].suit);
    }
  }

  // Insertion sort descending
  for (int i = 1; i < n; i++) {
    int key = faces[i];
    int j = i - 1;
    while (j >= 0 && faces[j] < key) {
      faces[j + 1] = faces[j];
      j--;
    }
    faces[j + 1] = key;
  }

  // Count occurrences (valid face values 2-14)
  int cnt[15] = {0};
  for (int i = 0; i < n; i++)
    if (faces[i] >= 2 && faces[i] <= 14)
      cnt[faces[i]]++;

  // Find best and second-best group face (highest count first, then highest face)
  int best_n = 1, best_f = 0;
  int second_n = 0, second_f = 0;
  for (int f = 14; f >= 2; f--) {
    if (cnt[f] < 2)
      continue;
    if (cnt[f] > best_n || (cnt[f] == best_n && f > best_f)) {
      second_n = best_n;
      second_f = best_f;
      best_n = cnt[f];
      best_f = f;
    } else if (second_f == 0 && cnt[f] >= 2) {
      second_n = cnt[f];
      second_f = f;
    }
  }
  (void)second_n;

  // hand_rank: quads=7, trips=6, two-pair=5, pair=4, high-card=3
  int hand_rank;
  if (best_n == 4)
    hand_rank = 7;
  else if (best_n == 3)
    hand_rank = 6;
  else if (best_n == 2 && second_f > 0)
    hand_rank = 5;
  else if (best_n == 2)
    hand_rank = 4;
  else
    hand_rank = 3;

  uint64_t score = (uint64_t)hand_rank << 48;
  score |= (uint64_t)best_f << 40;
  score |= (uint64_t)second_f << 32;
  for (int i = 0; i < 4 && i < n; i++)
    score |= (uint64_t)faces[i] << (24 - i * 8);
  score |= (uint64_t)best_suit;

  return score;
}

// Score 1-7 visible cards for betting-order comparison in no-peek games.
// Unified scale: (pokeval_rank+1) << 56 for hand rank; then 4-bit slots at
// bits [55:52], [51:48], [47:44] … for v0/v1/kickers (no suit bits, so
// equal-rank hands like J♥ and J♠ score identically).
//
// Slot layout (same for n≤4 and n≥5 so cross-n comparisons work correctly):
//   v0: group face for pairs/trips/quads, or highest face for non-group hands
//   v1: lower pair for two-pair/full-house, or 2nd-highest face for non-group
//   kickers: remaining faces, descending
uint64_t POKEVAL_score_visible_cards(const DH_Card *cards, int n) {
  if (n <= 0)
    return 0;

  if (n <= 4) {
    int faces[4] = {0};
    for (int i = 0; i < n; i++)
      faces[i] = face_rank(cards[i].face_val);

    // Insertion sort descending
    for (int i = 1; i < n; i++) {
      int key = faces[i], j = i - 1;
      while (j >= 0 && faces[j] < key) {
        faces[j + 1] = faces[j];
        j--;
      }
      faces[j + 1] = key;
    }

    int cnt[15] = {0};
    for (int i = 0; i < n; i++)
      if (faces[i] >= 2 && faces[i] <= 14)
        cnt[faces[i]]++;

    int best_n = 1, best_f = 0, second_n = 0, second_f = 0;
    for (int f = 14; f >= 2; f--) {
      if (cnt[f] < 2)
        continue;
      if (cnt[f] > best_n || (cnt[f] == best_n && f > best_f)) {
        second_n = best_n;
        second_f = best_f;
        best_n = cnt[f];
        best_f = f;
      } else if (second_f == 0 && cnt[f] >= 2) {
        second_n = cnt[f];
        second_f = f;
      }
    }
    (void)second_n;

    int pokeval_rank;
    if (best_n == 4)
      pokeval_rank = POKEVAL_FOUR_OF_A_KIND;
    else if (best_n == 3)
      pokeval_rank = POKEVAL_THREE_OF_A_KIND;
    else if (best_n == 2 && second_f > 0)
      pokeval_rank = POKEVAL_TWO_PAIR;
    else if (best_n == 2)
      pokeval_rank = POKEVAL_PAIR;
    else
      pokeval_rank = POKEVAL_HIGH_CARD;

    // v0/v1: group face or two highest cards for high card
    int v0, v1;
    int vk[4], nk = 0;
    if (pokeval_rank == POKEVAL_HIGH_CARD) {
      v0 = faces[0];
      v1 = (n >= 2) ? faces[1] : 0;
      for (int i = 2; i < n; i++)
        vk[nk++] = faces[i];
    } else {
      v0 = best_f;
      v1 = second_f;
      for (int i = 0; i < n; i++)
        if (faces[i] != best_f && faces[i] != second_f)
          vk[nk++] = faces[i];
    }

    uint64_t score = ((uint64_t)(pokeval_rank + 1)) << 56;
    score |= (uint64_t)(v0 & 0xF) << 52;
    score |= (uint64_t)(v1 & 0xF) << 48;
    for (int i = 0; i < nk; i++)
      score |= (uint64_t)(vk[i] & 0xF) << (44 - i * 4);
    return score;
  }

  POKEVAL_Hand_9 hand9 = {0};
  for (int i = 0; i < n; i++)
    hand9.card[i] = cards[i];
  for (int i = n; i < 9; i++)
    hand9.card[i] = DH_card_null;

  POKEVAL_Hand_5 best5 = POKEVAL_hand5_from_hand7(&hand9);
  short rank = POKEVAL_evaluate_hand(best5);
  POKEVAL_sort_hand(&best5);

  int v0 = 0, v1 = 0;
  int vk[5], nk = 0;

  if (rank == POKEVAL_HIGH_CARD || rank == POKEVAL_STRAIGHT || rank == POKEVAL_FLUSH ||
      rank == POKEVAL_STRAIGHT_FLUSH || rank == POKEVAL_ROYAL_FLUSH) {
    v0 = face_rank(best5.card[0].face_val);
    v1 = face_rank(best5.card[1].face_val);
    for (int i = 2; i < POKEVAL_HAND_SIZE; i++)
      vk[nk++] = face_rank(best5.card[i].face_val);
  } else {
    int gcnt[15] = {0};
    for (int i = 0; i < POKEVAL_HAND_SIZE; i++) {
      int f = face_rank(best5.card[i].face_val);
      if (f >= 2 && f <= 14)
        gcnt[f]++;
    }
    int target = (rank == POKEVAL_FIVE_OF_A_KIND)                                  ? 5
                 : (rank == POKEVAL_FOUR_OF_A_KIND)                                ? 4
                 : (rank == POKEVAL_THREE_OF_A_KIND || rank == POKEVAL_FULL_HOUSE) ? 3
                                                                                   : 2;
    for (int f = 14; f >= 2; f--) {
      if (gcnt[f] >= target) {
        v0 = f;
        break;
      }
    }
    if (rank == POKEVAL_TWO_PAIR || rank == POKEVAL_FULL_HOUSE) {
      for (int f = 14; f >= 2; f--) {
        if (f != v0 && gcnt[f] >= 2) {
          v1 = f;
          break;
        }
      }
    }
    for (int i = 0; i < POKEVAL_HAND_SIZE; i++) {
      int f = face_rank(best5.card[i].face_val);
      if (f != v0 && f != v1)
        vk[nk++] = f;
    }
  }

  uint64_t score = ((uint64_t)(rank + 1)) << 56;
  score |= (uint64_t)(v0 & 0xF) << 52;
  score |= (uint64_t)(v1 & 0xF) << 48;
  for (int i = 0; i < nk; i++)
    score |= (uint64_t)(vk[i] & 0xF) << (44 - i * 4);
  return score;
}
