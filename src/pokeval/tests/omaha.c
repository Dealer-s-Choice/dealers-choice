#include "00_test.h"

// Omaha layout: positions 0-3 are hole cards, positions 4-8 are community cards.
// Players MUST use exactly 2 hole cards and 3 community cards.

_MAIN_HEAD_

// --- Test 1: hand5_omaha returns the correct best hand (Full House) ---
//
// Hole:      A♠ A♥ 2♣ 3♦
// Community: K♦ K♥ K♣ Q♦ J♠
//
// Best 2+3: A♠A♥ (hole) + K♦K♥K♣ (community) = K K K A A → Full House

{
  POKEVAL_Hand_9 hand = {{
      {DH_CARD_ACE, DH_SUIT_SPADES},     // hole 0
      {DH_CARD_ACE, DH_SUIT_HEARTS},     // hole 1
      {DH_CARD_TWO, DH_SUIT_CLUBS},      // hole 2
      {DH_CARD_THREE, DH_SUIT_DIAMONDS}, // hole 3
      {DH_CARD_KING, DH_SUIT_DIAMONDS},  // community 0
      {DH_CARD_KING, DH_SUIT_HEARTS},    // community 1
      {DH_CARD_KING, DH_SUIT_CLUBS},     // community 2
      {DH_CARD_QUEEN, DH_SUIT_DIAMONDS}, // community 3
      {DH_CARD_JACK, DH_SUIT_SPADES},    // community 4
  }};

  POKEVAL_Hand_5 best = POKEVAL_hand5_omaha(&hand);
  short rank = POKEVAL_evaluate_hand(best);
  fprintf(stderr, "Test 1 (Full House): rank=%d expected=%d\n", rank, POKEVAL_FULL_HOUSE);
  assert(rank == POKEVAL_FULL_HOUSE);
}

// --- Test 2: Omaha constraint prevents Royal Flush ---
//
// Hole:      2♠ 3♠ 4♠ 5♠
// Community: A♠ K♠ Q♠ J♠ T♠
//
// Without constraint: A♠K♠Q♠J♠T♠ = Royal Flush.
// With Omaha rule (exactly 2 hole + 3 community): best possible is an
// Ace-high flush (e.g. 5♠4♠ + A♠K♠Q♠) — no straight flush is achievable.

{
  POKEVAL_Hand_9 hand = {{
      {DH_CARD_TWO, DH_SUIT_SPADES},   // hole 0
      {DH_CARD_THREE, DH_SUIT_SPADES}, // hole 1
      {DH_CARD_FOUR, DH_SUIT_SPADES},  // hole 2
      {DH_CARD_FIVE, DH_SUIT_SPADES},  // hole 3
      {DH_CARD_ACE, DH_SUIT_SPADES},   // community 0
      {DH_CARD_KING, DH_SUIT_SPADES},  // community 1
      {DH_CARD_QUEEN, DH_SUIT_SPADES}, // community 2
      {DH_CARD_JACK, DH_SUIT_SPADES},  // community 3
      {DH_CARD_TEN, DH_SUIT_SPADES},   // community 4
  }};

  POKEVAL_Hand_5 best = POKEVAL_hand5_omaha(&hand);
  short rank = POKEVAL_evaluate_hand(best);
  fprintf(stderr, "Test 2 (Flush, not Royal Flush): rank=%d expected=%d\n", rank, POKEVAL_FLUSH);
  assert(rank == POKEVAL_FLUSH);
}

// --- Test 3: compare_hands_omaha - straight beats three of a kind ---
//
// Community (shared): A♣ Q♥ J♣ T♦ 9♠
//
// Player 0 hole: A♠ K♠ 2♦ 3♦
//   Best: A♠K♠ + Q♥J♣T♦ = A-K-Q-J-T → Straight
//
// Player 1 hole: A♥ A♦ 5♣ 6♣
//   Best: A♥A♦ + A♣Q♥J♣ = A-A-A-Q-J → Three of a Kind
//
// Straight > Three of a Kind → Player 0 wins.

{
  POKEVAL_Hand_9 h0 = {{
      {DH_CARD_ACE, DH_SUIT_SPADES},     // hole 0
      {DH_CARD_KING, DH_SUIT_SPADES},    // hole 1
      {DH_CARD_TWO, DH_SUIT_DIAMONDS},   // hole 2
      {DH_CARD_THREE, DH_SUIT_DIAMONDS}, // hole 3
      {DH_CARD_ACE, DH_SUIT_CLUBS},      // community 0
      {DH_CARD_QUEEN, DH_SUIT_HEARTS},   // community 1
      {DH_CARD_JACK, DH_SUIT_CLUBS},     // community 2
      {DH_CARD_TEN, DH_SUIT_DIAMONDS},   // community 3
      {DH_CARD_NINE, DH_SUIT_SPADES},    // community 4
  }};

  POKEVAL_Hand_9 h1 = {{
      {DH_CARD_ACE, DH_SUIT_HEARTS},   // hole 0
      {DH_CARD_ACE, DH_SUIT_DIAMONDS}, // hole 1
      {DH_CARD_FIVE, DH_SUIT_CLUBS},   // hole 2
      {DH_CARD_SIX, DH_SUIT_CLUBS},    // hole 3
      {DH_CARD_ACE, DH_SUIT_CLUBS},    // community 0
      {DH_CARD_QUEEN, DH_SUIT_HEARTS}, // community 1
      {DH_CARD_JACK, DH_SUIT_CLUBS},   // community 2
      {DH_CARD_TEN, DH_SUIT_DIAMONDS}, // community 3
      {DH_CARD_NINE, DH_SUIT_SPADES},  // community 4
  }};

  POKEVAL_NeedComparing nc[2] = {
      {.id = 0, .hand = h0},
      {.id = 1, .hand = h1},
  };

  uint8_t n_wins = POKEVAL_compare_hands_omaha(nc, 2);
  fprintf(stderr, "Test 3 (Straight beats Trips): n_wins=%d\n", n_wins);
  assert(n_wins == 1);
  assert(nc[0].won); // Player 0 wins with Straight
  assert(!nc[1].won);

  // Double-check the actual ranks
  assert(POKEVAL_evaluate_hand(nc[0].hand_5) == POKEVAL_STRAIGHT);
  assert(POKEVAL_evaluate_hand(nc[1].hand_5) == POKEVAL_THREE_OF_A_KIND);
}

// --- Test 4: compare_hands_omaha - tie (same ace-high straight) ---
//
// Community (shared): Q♦ J♠ T♥ 9♣ 8♦
//
// Player 0 hole: A♠ K♠ 2♣ 3♣  →  A♠K♠ + Q♦J♠T♥ = A-K-Q-J-T straight
// Player 1 hole: A♥ K♥ 4♦ 5♦  →  A♥K♥ + Q♦J♠T♥ = A-K-Q-J-T straight
//
// Both make the same ace-high straight → split pot, both win.

{
  POKEVAL_Hand_9 h0 = {{
      {DH_CARD_ACE, DH_SUIT_SPADES},     // hole 0
      {DH_CARD_KING, DH_SUIT_SPADES},    // hole 1
      {DH_CARD_TWO, DH_SUIT_CLUBS},      // hole 2
      {DH_CARD_THREE, DH_SUIT_CLUBS},    // hole 3
      {DH_CARD_QUEEN, DH_SUIT_DIAMONDS}, // community 0
      {DH_CARD_JACK, DH_SUIT_SPADES},    // community 1
      {DH_CARD_TEN, DH_SUIT_HEARTS},     // community 2
      {DH_CARD_NINE, DH_SUIT_CLUBS},     // community 3
      {DH_CARD_EIGHT, DH_SUIT_DIAMONDS}, // community 4
  }};

  POKEVAL_Hand_9 h1 = {{
      {DH_CARD_ACE, DH_SUIT_HEARTS},     // hole 0
      {DH_CARD_KING, DH_SUIT_HEARTS},    // hole 1
      {DH_CARD_FOUR, DH_SUIT_DIAMONDS},  // hole 2
      {DH_CARD_FIVE, DH_SUIT_DIAMONDS},  // hole 3
      {DH_CARD_QUEEN, DH_SUIT_DIAMONDS}, // community 0
      {DH_CARD_JACK, DH_SUIT_SPADES},    // community 1
      {DH_CARD_TEN, DH_SUIT_HEARTS},     // community 2
      {DH_CARD_NINE, DH_SUIT_CLUBS},     // community 3
      {DH_CARD_EIGHT, DH_SUIT_DIAMONDS}, // community 4
  }};

  POKEVAL_NeedComparing nc[2] = {
      {.id = 0, .hand = h0},
      {.id = 1, .hand = h1},
  };

  uint8_t n_wins = POKEVAL_compare_hands_omaha(nc, 2);
  fprintf(stderr, "Test 4 (Tie - same straight): n_wins=%d\n", n_wins);
  assert(n_wins == 2);
  assert(nc[0].won);
  assert(nc[1].won);
}

_MAIN_TAIL_
