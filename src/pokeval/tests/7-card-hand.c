#include "00_test.h"

typedef struct {
  POKEVAL_Hand_9 hand;
  short expected_rank;
  const int *expected_cards; // NULL = don't check; otherwise sorted descending face values
  const char *description;
} TestCase;

_MAIN_HEAD_

TestCase cases[] = {
    {
        .hand = {{
            {DH_CARD_QUEEN, DH_SUIT_HEARTS},
            {DH_CARD_QUEEN, DH_SUIT_SPADES},
            {DH_CARD_QUEEN, DH_SUIT_DIAMONDS},
            {DH_CARD_TEN, DH_SUIT_CLUBS},
            {DH_CARD_TEN, DH_SUIT_HEARTS},
            {DH_CARD_FOUR, DH_SUIT_SPADES},
            {DH_CARD_TWO, DH_SUIT_CLUBS},
        }},
        .expected_rank = POKEVAL_FULL_HOUSE,
        .description = "Full House: Queens over Tens",
    },
    {
        .hand = {{
            {DH_CARD_FIVE, DH_SUIT_CLUBS},
            {DH_CARD_SEVEN, DH_SUIT_CLUBS},
            {DH_CARD_NINE, DH_SUIT_CLUBS},
            {DH_CARD_TWO, DH_SUIT_DIAMONDS},
            {DH_CARD_SIX, DH_SUIT_CLUBS},
            {DH_CARD_KING, DH_SUIT_HEARTS},
            {DH_CARD_EIGHT, DH_SUIT_CLUBS},
        }},
        .expected_rank = POKEVAL_STRAIGHT_FLUSH,
        .description = "Straight Flush",
    },
    {
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_SPADES},
            {DH_CARD_KING, DH_SUIT_HEARTS},
            {DH_CARD_QUEEN, DH_SUIT_CLUBS},
            {DH_CARD_JACK, DH_SUIT_SPADES},
            {DH_CARD_TEN, DH_SUIT_DIAMONDS},
            {DH_CARD_THREE, DH_SUIT_CLUBS},
            {DH_CARD_TWO, DH_SUIT_HEARTS},
        }},
        .expected_rank = POKEVAL_STRAIGHT,
        .description = "Ace-high Straight",
    },
    {
        .hand = {{
            {DH_CARD_THREE, DH_SUIT_SPADES},
            {DH_CARD_THREE, DH_SUIT_HEARTS},
            {DH_CARD_THREE, DH_SUIT_CLUBS},
            {DH_CARD_FIVE, DH_SUIT_DIAMONDS},
            {DH_CARD_EIGHT, DH_SUIT_CLUBS},
            {DH_CARD_QUEEN, DH_SUIT_SPADES},
            {DH_CARD_KING, DH_SUIT_HEARTS},
        }},
        .expected_rank = POKEVAL_THREE_OF_A_KIND,
        .description = "Three of a Kind: Threes",
    },
    {
        .hand = {{
            {DH_CARD_FIVE, DH_SUIT_DIAMONDS},
            {DH_CARD_EIGHT, DH_SUIT_CLUBS},
            {DH_CARD_ACE, DH_SUIT_SPADES},
            {DH_CARD_FIVE, DH_SUIT_HEARTS},
            {DH_CARD_THREE, DH_SUIT_SPADES},
            {DH_CARD_THREE, DH_SUIT_CLUBS},
            {DH_CARD_THREE, DH_SUIT_HEARTS},
        }},
        .expected_rank = POKEVAL_FULL_HOUSE,
        .description = "Full House",
    },
    {
        .hand = {{
            {DH_CARD_KING, DH_SUIT_SPADES},
            {DH_CARD_KING, DH_SUIT_HEARTS},
            {DH_CARD_KING, DH_SUIT_DIAMONDS},
            {DH_CARD_TWO, DH_SUIT_HEARTS},
            {DH_CARD_ACE, DH_SUIT_CLUBS},
            {DH_CARD_THREE, DH_SUIT_SPADES},
            {DH_CARD_JACK, DH_SUIT_CLUBS},
        }},
        .expected_rank = POKEVAL_THREE_OF_A_KIND,
        .expected_cards =
            (const int[]){POKEVAL_ACE, DH_CARD_KING, DH_CARD_KING, DH_CARD_KING, DH_CARD_JACK},
        .description = "Three of a Kind: Kings (Ace kicker must be selected, not Three)",
    },
    {
        // 6 hearts: A K Q J 9 3, plus 2 of diamonds
        // Best flush must take the 5 highest: A K Q J 9 (drop 3)
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_HEARTS},
            {DH_CARD_KING, DH_SUIT_HEARTS},
            {DH_CARD_QUEEN, DH_SUIT_HEARTS},
            {DH_CARD_JACK, DH_SUIT_HEARTS},
            {DH_CARD_NINE, DH_SUIT_HEARTS},
            {DH_CARD_THREE, DH_SUIT_HEARTS},
            {DH_CARD_TWO, DH_SUIT_DIAMONDS},
        }},
        .expected_rank = POKEVAL_FLUSH,
        .expected_cards =
            (const int[]){POKEVAL_ACE, DH_CARD_KING, DH_CARD_QUEEN, DH_CARD_JACK, DH_CARD_NINE},
        .description = "Flush: 6 same-suit cards, picks 5 highest",
    },
    {
        // Three aces, two kings, two queens -> Full House Aces over Kings (not Aces over Queens)
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_SPADES},
            {DH_CARD_ACE, DH_SUIT_HEARTS},
            {DH_CARD_ACE, DH_SUIT_DIAMONDS},
            {DH_CARD_KING, DH_SUIT_CLUBS},
            {DH_CARD_KING, DH_SUIT_HEARTS},
            {DH_CARD_QUEEN, DH_SUIT_SPADES},
            {DH_CARD_QUEEN, DH_SUIT_DIAMONDS},
        }},
        .expected_rank = POKEVAL_FULL_HOUSE,
        .expected_cards =
            (const int[]){POKEVAL_ACE, POKEVAL_ACE, POKEVAL_ACE, DH_CARD_KING, DH_CARD_KING},
        .description = "Full House: Aces over Kings (must pick Kings over Queens as the pair)",
    },
    {
        // Two pair A A K K with Q J 2 remaining -> Queen kicker selected, not Jack or Two
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_SPADES},
            {DH_CARD_ACE, DH_SUIT_HEARTS},
            {DH_CARD_KING, DH_SUIT_CLUBS},
            {DH_CARD_KING, DH_SUIT_DIAMONDS},
            {DH_CARD_QUEEN, DH_SUIT_SPADES},
            {DH_CARD_JACK, DH_SUIT_HEARTS},
            {DH_CARD_TWO, DH_SUIT_CLUBS},
        }},
        .expected_rank = POKEVAL_TWO_PAIR,
        .expected_cards =
            (const int[]){POKEVAL_ACE, POKEVAL_ACE, DH_CARD_KING, DH_CARD_KING, DH_CARD_QUEEN},
        .description = "Two Pair: A A K K with Queen kicker (not Jack or Two)",
    },
    {
        // A 2 3 4 5 6 7 — three overlapping straights; highest is 3-4-5-6-7
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_SPADES},
            {DH_CARD_TWO, DH_SUIT_HEARTS},
            {DH_CARD_THREE, DH_SUIT_DIAMONDS},
            {DH_CARD_FOUR, DH_SUIT_CLUBS},
            {DH_CARD_FIVE, DH_SUIT_SPADES},
            {DH_CARD_SIX, DH_SUIT_HEARTS},
            {DH_CARD_SEVEN, DH_SUIT_DIAMONDS},
        }},
        .expected_rank = POKEVAL_STRAIGHT,
        .expected_cards =
            (const int[]){DH_CARD_SEVEN, DH_CARD_SIX, DH_CARD_FIVE, DH_CARD_FOUR, DH_CARD_THREE},
        .description = "Straight: A-2-3-4-5-6-7 picks highest (3-4-5-6-7)",
    },
    {
        // Four aces with K Q as kickers -> King kicker selected, not Queen
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_SPADES},
            {DH_CARD_ACE, DH_SUIT_HEARTS},
            {DH_CARD_ACE, DH_SUIT_DIAMONDS},
            {DH_CARD_ACE, DH_SUIT_CLUBS},
            {DH_CARD_KING, DH_SUIT_SPADES},
            {DH_CARD_QUEEN, DH_SUIT_HEARTS},
            {DH_CARD_TWO, DH_SUIT_DIAMONDS},
        }},
        .expected_rank = POKEVAL_FOUR_OF_A_KIND,
        .expected_cards =
            (const int[]){POKEVAL_ACE, POKEVAL_ACE, POKEVAL_ACE, POKEVAL_ACE, DH_CARD_KING},
        .description = "Four of a Kind: Aces with King kicker (not Queen)",
    },
    {
        // One pair of Aces, kickers K Q J 9 2 -> best 3 kickers are K Q J
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_SPADES},
            {DH_CARD_ACE, DH_SUIT_HEARTS},
            {DH_CARD_KING, DH_SUIT_CLUBS},
            {DH_CARD_QUEEN, DH_SUIT_DIAMONDS},
            {DH_CARD_JACK, DH_SUIT_SPADES},
            {DH_CARD_NINE, DH_SUIT_HEARTS},
            {DH_CARD_TWO, DH_SUIT_CLUBS},
        }},
        .expected_rank = POKEVAL_PAIR,
        .expected_cards =
            (const int[]){POKEVAL_ACE, POKEVAL_ACE, DH_CARD_KING, DH_CARD_QUEEN, DH_CARD_JACK},
        .description = "One Pair: Aces with K Q J kickers (not Nine or Two)",
    },
    {
        // 5-6-7-8-9-K-A all spades: straight flush (5-9) beats plain flush
        .hand = {{
            {DH_CARD_FIVE, DH_SUIT_SPADES},
            {DH_CARD_SIX, DH_SUIT_SPADES},
            {DH_CARD_SEVEN, DH_SUIT_SPADES},
            {DH_CARD_EIGHT, DH_SUIT_SPADES},
            {DH_CARD_NINE, DH_SUIT_SPADES},
            {DH_CARD_KING, DH_SUIT_SPADES},
            {DH_CARD_ACE, DH_SUIT_SPADES},
        }},
        .expected_rank = POKEVAL_STRAIGHT_FLUSH,
        .expected_cards =
            (const int[]){DH_CARD_NINE, DH_CARD_EIGHT, DH_CARD_SEVEN, DH_CARD_SIX, DH_CARD_FIVE},
        .description = "Straight Flush beats Flush when both present (5-9 of spades)",
    },
};

size_t num_cases = sizeof cases / sizeof cases[0];

for (size_t i = 0; i < num_cases; ++i) {
  POKEVAL_Hand_5 reduced = POKEVAL_hand5_from_hand7(&cases[i].hand);
  for (int j = 0; j < POKEVAL_HAND_SIZE; j++)
    fprintf(stderr, "card: %d | ", reduced.card[j].face_val);
  fputc('\n', stderr);

  short actual_rank = POKEVAL_evaluate_hand(reduced);

  fprintf(stderr, "Test %zu: %s (Expected rank: %d, Got: %d)\n", i + 1, cases[i].description,
          cases[i].expected_rank, actual_rank);

  fputc('\n', stderr);

  assert(actual_rank == cases[i].expected_rank);

  if (cases[i].expected_cards) {
    for (int j = 0; j < POKEVAL_HAND_SIZE; j++)
      assert(reduced.card[j].face_val == cases[i].expected_cards[j]);
  }
}

_MAIN_TAIL_
