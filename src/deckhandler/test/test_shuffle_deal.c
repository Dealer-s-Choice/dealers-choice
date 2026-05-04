/*
 * test_01.c
 *
 * This file is part of the deckhandler library
 * <https://github.com/theimpossibleastronaut/deckhandler>
 *
 * Copyright 2019-2021 Andy Alt <andy400-dev@yahoo.com>
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
 *
 *
 */

#ifdef NDEBUG
#undef NDEBUG
#endif
#include "deckhandler.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  /* declare a deck using type "DH_Deck" (defined in deckhandler.h) */
  DH_Deck deck_a = DH_get_new_deck();

  int deals_num = 0;
  int deals_max = 3;

  DH_pcg_srand(1, 1);

  do {
    /* shuffle the deck */
    puts("\n\n\t]=[ Shuffling ]=[\n");
    DH_shuffle_deck(&deck_a);

    /* show each card in the deck */
    int deal;
    for (deal = 0; deal < DH_CARDS_IN_DECK; deal++) {
      /* The "faces" and "suits" arrays are initialized in deckhandler.c */
      // fprintf (stderr, "%s of %s\n", DH_get_card_face(deck_a.card[deal]),
      // DH_get_card_suit(deck_a.card[deal]));
      DH_Card card = DH_deal_top_card(&deck_a);
      fprintf(stderr, "%i\n", card.face_val);
      if (deal < 4 && deals_num == 0) {
        switch (deal) {
        case 0:
          assert(card.face_val == DH_CARD_TWO);
          break;
        case 1:
          assert(card.face_val == DH_CARD_FIVE);
          break;
        case 2:
          assert(card.face_val == DH_CARD_FIVE);
          break;
        case 3:
          assert(card.face_val == DH_CARD_FIVE);
          break;
        case 4:
          assert(card.face_val == DH_CARD_QUEEN);
          break;
        case 5:
          assert(card.face_val == DH_CARD_JACK);
          break;
        case 6:
          assert(card.face_val == DH_CARD_KING);
          break;
        case 7:
          assert(card.face_val == DH_CARD_SIX);
          break;
        case 8:
          assert(card.face_val == DH_CARD_NINE);
          break;
        case 9:
          assert(card.face_val == DH_CARD_THREE);
          break;
        }
      }
    }
  } while (deals_num++ < deals_max);

  puts("\n\n\t]=[ Create 4 decks, shuffle each one, and deal them all out ]=[\n");
  enum { total_decks = 4 };
  DH_Deck deck_num[total_decks];
  for (int i = 0; i < total_decks; i++)
    deck_num[i] = DH_get_new_deck();

  int which_deck;
  for (which_deck = 0; which_deck < total_decks; which_deck++)
    DH_shuffle_deck(&deck_num[which_deck]);

  /* There's no function in the library (yet) that shuffles multiple decks
   * together, but using the method below will provide a close simulation */
  for (int i = 0; i < DH_CARDS_IN_DECK; i++) {
    for (which_deck = 0; which_deck < total_decks; which_deck++) {
      DH_Card card = DH_deal_top_card(&deck_num[which_deck]);
      printf("%s of %s\n", DH_get_card_face(card), DH_get_card_suit(card));
      if (i == DH_CARDS_IN_DECK - 1) {
        switch (which_deck) {
        case 0:
          assert(card.face_val == DH_CARD_TEN);
          break;
        case 1:
          assert(card.face_val == DH_CARD_TWO);
          break;
        case 2:
          assert(card.face_val == DH_CARD_THREE);
          break;
        case 3:
          assert(card.face_val == DH_CARD_ACE);
          break;
        }
      }
    }
  }

  return 0;
}
