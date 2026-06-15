/*
 * test_recycle.c
 *
 * This file is part of the deckhandler library
 *
 * Exercises the discard-pile recycling path: when the draw pile is exhausted,
 * the deal function reshuffles the discard pile (muck) and deals from it. When
 * both piles are empty it returns DH_card_null instead of silently re-dealing
 * already-dealt cards.
 */

#ifdef NDEBUG
#undef NDEBUG
#endif
#include "deckhandler.h"
#include <assert.h>
#include <stdio.h>

static bool card_eq(DH_Card a, DH_Card b) { return a.face_val == b.face_val && a.suit == b.suit; }

int main(void) {
  DH_pcg_srand(1, 1);
  DH_Deck deck = DH_get_new_deck();

  /* A fresh deck has all 52 cards available. */
  assert(DH_cards_remaining(&deck) == DH_CARDS_IN_DECK);

  /* Deal the whole deck, recording every card. */
  DH_Card dealt[DH_CARDS_IN_DECK];
  for (int i = 0; i < DH_CARDS_IN_DECK; i++) {
    dealt[i] = DH_deal_top_card(&deck);
    assert(!DH_is_card_null(dealt[i]));
  }
  assert(DH_cards_remaining(&deck) == 0);

  /* With the draw pile empty and no muck, the next deal must be null --
   * NOT a silent wrap that re-deals a card already in play. */
  assert(DH_is_card_null(DH_deal_top_card(&deck)));

  /* Muck five known cards back. */
  for (int i = 0; i < 5; i++)
    DH_discard_card(&deck, dealt[i]);
  assert(DH_cards_remaining(&deck) == 5);

  /* Deal two from the recycled muck... */
  DH_Card got[7];
  int n_got = 0;
  got[n_got++] = DH_deal_top_card(&deck);
  got[n_got++] = DH_deal_top_card(&deck);
  assert(DH_cards_remaining(&deck) == 3);

  /* ...then discard two MORE while still dealing from the muck (the deck-ran-
   * out-mid-draw case). The pile is shuffled once at exhaustion; these later
   * discards are appended and still dealt out -- nothing gets lost. */
  DH_discard_card(&deck, dealt[5]);
  DH_discard_card(&deck, dealt[6]);
  assert(DH_cards_remaining(&deck) == 5);

  /* Drain the rest. */
  while (DH_cards_remaining(&deck) > 0) {
    DH_Card c = DH_deal_top_card(&deck);
    assert(!DH_is_card_null(c));
    got[n_got++] = c;
  }
  assert(n_got == 7);

  /* The seven recycled cards are exactly the seven discarded, each once. */
  for (int i = 0; i < 7; i++) {
    int from_muck = 0, dup = 0;
    for (int j = 0; j < 7; j++)
      if (card_eq(got[i], dealt[j]))
        from_muck++;
    for (int j = 0; j < 7; j++)
      if (j != i && card_eq(got[i], got[j]))
        dup++;
    assert(from_muck == 1); /* every dealt card was a discarded one */
    assert(dup == 0);       /* and no duplicates */
  }

  /* Muck drained -> null. */
  assert(DH_is_card_null(DH_deal_top_card(&deck)));

  /* Multi-draw stress: 4 hands of 5, then many full discard/redraw rounds.
   * Cumulative discards far exceed 52 (the case that overflowed a fixed index),
   * driving repeated muck recycles. No card in play may ever duplicate, and the
   * deck must never run dry (each draw is preceded by a discard). */
  DH_shuffle_deck(&deck);
  enum { P = 4, H = 5 };
  DH_Card hand[P][H];
  for (int p = 0; p < P; p++)
    for (int c = 0; c < H; c++) {
      hand[p][c] = DH_deal_top_card(&deck);
      assert(!DH_is_card_null(hand[p][c]));
    }

  for (int round = 0; round < 6; round++) {
    for (int p = 0; p < P; p++)
      for (int c = 0; c < H; c++) {
        DH_discard_card(&deck, hand[p][c]);
        DH_Card nc = DH_deal_top_card(&deck);
        assert(!DH_is_card_null(nc));
        hand[p][c] = nc;
      }
    for (int a = 0; a < P * H; a++)
      for (int b = a + 1; b < P * H; b++)
        assert(!card_eq(hand[a / H][a % H], hand[b / H][b % H]));
  }

  puts("recycle test passed");
  return 0;
}
