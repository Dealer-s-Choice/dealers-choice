#include "00_test.h"

#define NULL_CARD {DH_CARD_NULL, DH_CARD_NULL}

_MAIN_HEAD_

TestCase cases[] = {
    {
        // 6 hearts: A K Q J 9 3 — pick 5 highest (drop 3)
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_HEARTS},
            {DH_CARD_KING, DH_SUIT_HEARTS},
            {DH_CARD_QUEEN, DH_SUIT_HEARTS},
            {DH_CARD_JACK, DH_SUIT_HEARTS},
            {DH_CARD_NINE, DH_SUIT_HEARTS},
            {DH_CARD_THREE, DH_SUIT_HEARTS},
            NULL_CARD,
        }},
        .expected_rank = POKEVAL_FLUSH,
        .expected_cards =
            (const int[]){POKEVAL_ACE, DH_CARD_KING, DH_CARD_QUEEN, DH_CARD_JACK, DH_CARD_NINE},
        .description = "Flush: 6 same-suit cards, picks 5 highest (drop Three)",
    },
    {
        // K K K A Q 3 — three Kings, best kickers are Ace and Queen (drop Three)
        .hand = {{
            {DH_CARD_KING, DH_SUIT_SPADES},
            {DH_CARD_KING, DH_SUIT_HEARTS},
            {DH_CARD_KING, DH_SUIT_DIAMONDS},
            {DH_CARD_ACE, DH_SUIT_CLUBS},
            {DH_CARD_QUEEN, DH_SUIT_SPADES},
            {DH_CARD_THREE, DH_SUIT_HEARTS},
            NULL_CARD,
        }},
        .expected_rank = POKEVAL_THREE_OF_A_KIND,
        .expected_cards =
            (const int[]){POKEVAL_ACE, DH_CARD_KING, DH_CARD_KING, DH_CARD_KING, DH_CARD_QUEEN},
        .description = "Three of a Kind: Kings, Ace+Queen kickers (drop Three)",
    },
    {
        // A 2 3 4 5 6 — two possible straights (A-2-3-4-5 wheel and 2-3-4-5-6); pick 2-3-4-5-6
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_SPADES},
            {DH_CARD_TWO, DH_SUIT_HEARTS},
            {DH_CARD_THREE, DH_SUIT_DIAMONDS},
            {DH_CARD_FOUR, DH_SUIT_CLUBS},
            {DH_CARD_FIVE, DH_SUIT_SPADES},
            {DH_CARD_SIX, DH_SUIT_HEARTS},
            NULL_CARD,
        }},
        .expected_rank = POKEVAL_STRAIGHT,
        .expected_cards =
            (const int[]){DH_CARD_SIX, DH_CARD_FIVE, DH_CARD_FOUR, DH_CARD_THREE, DH_CARD_TWO},
        .description = "Straight: A-2-3-4-5-6 picks highest (2-3-4-5-6, not wheel)",
    },
    {
        // A A A K K Q — Full House Aces over Kings (drop Queen)
        .hand = {{
            {DH_CARD_ACE, DH_SUIT_SPADES},
            {DH_CARD_ACE, DH_SUIT_HEARTS},
            {DH_CARD_ACE, DH_SUIT_DIAMONDS},
            {DH_CARD_KING, DH_SUIT_CLUBS},
            {DH_CARD_KING, DH_SUIT_HEARTS},
            {DH_CARD_QUEEN, DH_SUIT_SPADES},
            NULL_CARD,
        }},
        .expected_rank = POKEVAL_FULL_HOUSE,
        .expected_cards =
            (const int[]){POKEVAL_ACE, POKEVAL_ACE, POKEVAL_ACE, DH_CARD_KING, DH_CARD_KING},
        .description = "Full House: Aces over Kings (drop Queen)",
    },
    {
        // 5-6-7-8-9-K all spades: straight flush (5-9) beats plain flush
        .hand = {{
            {DH_CARD_FIVE, DH_SUIT_SPADES},
            {DH_CARD_SIX, DH_SUIT_SPADES},
            {DH_CARD_SEVEN, DH_SUIT_SPADES},
            {DH_CARD_EIGHT, DH_SUIT_SPADES},
            {DH_CARD_NINE, DH_SUIT_SPADES},
            {DH_CARD_KING, DH_SUIT_SPADES},
            NULL_CARD,
        }},
        .expected_rank = POKEVAL_STRAIGHT_FLUSH,
        .expected_cards =
            (const int[]){DH_CARD_NINE, DH_CARD_EIGHT, DH_CARD_SEVEN, DH_CARD_SIX, DH_CARD_FIVE},
        .description = "Straight Flush beats Flush when both present in 6-card hand",
    },
};

run_hand7_rank_cases(cases, sizeof cases / sizeof cases[0]);

_MAIN_TAIL_
