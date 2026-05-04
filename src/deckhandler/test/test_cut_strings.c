/*
 * test_cut_strings.c
 *
 * This file is part of the deckhandler library
 * <https://github.com/theimpossibleastronaut/deckhandler>
 *
 * Copyright 2025 Andy Alt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifdef NDEBUG
#undef NDEBUG
#endif

#include "deckhandler.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*
 * Unshuffled deck layout from DH_init_deck():
 *   cards  0-12: Ace-King of Hearts   (suit 0)
 *   cards 13-25: Ace-King of Diamonds (suit 1)
 *   cards 26-38: Ace-King of Spades   (suit 2)
 *   cards 39-51: Ace-King of Clubs    (suit 3)
 */

static void test_is_card_back_null(void) {
  assert(DH_is_card_back(DH_card_back));
  assert(!DH_is_card_back(DH_card_null));

  assert(DH_is_card_null(DH_card_null));
  assert(!DH_is_card_null(DH_card_back));

  DH_Card regular = {.face_val = DH_CARD_ACE, .suit = DH_SUIT_HEARTS};
  assert(!DH_is_card_back(regular));
  assert(!DH_is_card_null(regular));
}

static void test_get_card_face(void) {
  DH_Card c = {.face_val = DH_CARD_ACE, .suit = DH_SUIT_HEARTS};
  assert(strcmp(DH_get_card_face(c), "Ace") == 0);

  c.face_val = DH_CARD_TEN;
  assert(strcmp(DH_get_card_face(c), "10") == 0);

  c.face_val = DH_CARD_JACK;
  assert(strcmp(DH_get_card_face(c), "Jack") == 0);

  c.face_val = DH_CARD_QUEEN;
  assert(strcmp(DH_get_card_face(c), "Queen") == 0);

  c.face_val = DH_CARD_KING;
  assert(strcmp(DH_get_card_face(c), "King") == 0);

  c.face_val = DH_CARD_ACE_HIGH;
  assert(strcmp(DH_get_card_face(c), "Ace") == 0);
}

static void test_get_card_suit(void) {
  DH_Card c = {.face_val = DH_CARD_TWO, .suit = DH_SUIT_HEARTS};
  assert(strcmp(DH_get_card_suit(c), "Hearts  ") == 0);

  c.suit = DH_SUIT_DIAMONDS;
  assert(strcmp(DH_get_card_suit(c), "Diamonds") == 0);

  c.suit = DH_SUIT_SPADES;
  assert(strcmp(DH_get_card_suit(c), "Spades  ") == 0);

  c.suit = DH_SUIT_CLUBS;
  assert(strcmp(DH_get_card_suit(c), "Clubs   ") == 0);
}

static void test_get_unicode_suit(void) {
  assert(strcmp(DH_get_unicode_suit(DH_SUIT_HEARTS),   "♥") == 0);
  assert(strcmp(DH_get_unicode_suit(DH_SUIT_DIAMONDS), "♦") == 0);
  assert(strcmp(DH_get_unicode_suit(DH_SUIT_SPADES),   "♠") == 0);
  assert(strcmp(DH_get_unicode_suit(DH_SUIT_CLUBS),    "♣") == 0);
  assert(strcmp(DH_get_unicode_suit(DH_SUIT_MAX),      "?") == 0);

  DH_Card c = {.face_val = DH_CARD_ACE, .suit = DH_SUIT_HEARTS};
  assert(strcmp(DH_get_card_unicode_suit(c), "♥") == 0);
}

static void test_get_card_face_str(void) {
  assert(strcmp(DH_get_card_face_str(DH_CARD_ACE),      "A")  == 0);
  assert(strcmp(DH_get_card_face_str(DH_CARD_TWO),      "2")  == 0);
  assert(strcmp(DH_get_card_face_str(DH_CARD_TEN),      "10") == 0);
  assert(strcmp(DH_get_card_face_str(DH_CARD_JACK),     "J")  == 0);
  assert(strcmp(DH_get_card_face_str(DH_CARD_QUEEN),    "Q")  == 0);
  assert(strcmp(DH_get_card_face_str(DH_CARD_KING),     "K")  == 0);
  assert(strcmp(DH_get_card_face_str(DH_CARD_ACE_HIGH), "A")  == 0);
  assert(strcmp(DH_get_card_face_str(0),                "?")  == 0);
  assert(strcmp(DH_get_card_face_str(15),               "?")  == 0);
}

static void test_deal_wrap(void) {
  DH_Deck deck = DH_get_new_deck();
  for (int i = 0; i < DH_CARDS_IN_DECK; i++)
    DH_deal_top_card(&deck);

  /* top_card == 52 here; next deal must wrap back to position 0 */
  DH_Card c = DH_deal_top_card(&deck);
  assert(c.face_val == DH_CARD_ACE);
  assert(c.suit == DH_SUIT_HEARTS);
}

static void test_cut_deck(void) {
  DH_Deck deck;

  /* cut at 26 (midpoint): bottom half becomes top */
  deck = DH_get_new_deck();
  DH_cut_deck(&deck, 26);
  assert(deck.card[0].face_val  == DH_CARD_ACE  && deck.card[0].suit  == DH_SUIT_SPADES);
  assert(deck.card[25].face_val == DH_CARD_KING  && deck.card[25].suit == DH_SUIT_CLUBS);
  assert(deck.card[26].face_val == DH_CARD_ACE   && deck.card[26].suit == DH_SUIT_HEARTS);
  assert(deck.card[51].face_val == DH_CARD_KING  && deck.card[51].suit == DH_SUIT_DIAMONDS);

  /* cut at 1: all but the first card move to the top */
  deck = DH_get_new_deck();
  DH_cut_deck(&deck, 1);
  assert(deck.card[0].face_val  == DH_CARD_TWO  && deck.card[0].suit  == DH_SUIT_HEARTS);
  assert(deck.card[51].face_val == DH_CARD_ACE  && deck.card[51].suit == DH_SUIT_HEARTS);

  /* cut at 51: only the last card moves to the top */
  deck = DH_get_new_deck();
  DH_cut_deck(&deck, 51);
  assert(deck.card[0].face_val == DH_CARD_KING  && deck.card[0].suit == DH_SUIT_CLUBS);
  assert(deck.card[1].face_val == DH_CARD_ACE   && deck.card[1].suit == DH_SUIT_HEARTS);

  /* boundary: cut at 0 is a no-op */
  deck = DH_get_new_deck();
  DH_cut_deck(&deck, 0);
  assert(deck.card[0].face_val == DH_CARD_ACE && deck.card[0].suit == DH_SUIT_HEARTS);

  /* boundary: cut at DH_CARDS_IN_DECK is a no-op */
  deck = DH_get_new_deck();
  DH_cut_deck(&deck, DH_CARDS_IN_DECK);
  assert(deck.card[0].face_val == DH_CARD_ACE && deck.card[0].suit == DH_SUIT_HEARTS);
}

static void test_pcg_srand_auto(void) {
  DH_pcg_srand_auto();
  DH_Deck deck = DH_get_new_deck();
  DH_shuffle_deck(&deck);

  /* verify all 52 cards are present: each face value appears exactly 4 times */
  int face_count[14] = {0};
  for (int i = 0; i < DH_CARDS_IN_DECK; i++) {
    int f = deck.card[i].face_val;
    assert(f >= DH_CARD_ACE && f <= DH_CARD_KING);
    face_count[f]++;
  }
  for (int f = DH_CARD_ACE; f <= DH_CARD_KING; f++)
    assert(face_count[f] == 4);
}

int main(void) {
  test_is_card_back_null();
  test_get_card_face();
  test_get_card_suit();
  test_get_unicode_suit();
  test_get_card_face_str();
  test_deal_wrap();
  test_cut_deck();
  test_pcg_srand_auto();

  puts("test_cut_strings: all assertions passed");
  return 0;
}
