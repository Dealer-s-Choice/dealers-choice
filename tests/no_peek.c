#include "00_test.h"

_MAIN_HEAD_

// --- game_choices[SEVEN_CARD_NO_PEEK] config ---

const GameChoice_t *choice = &game_choices[SEVEN_CARD_NO_PEEK];
assert(choice->game_type == 0x0A);
assert(choice->hand_size == 7);
assert(choice->n_cards_initial_deal == 7);
assert(choice->n_draws == 0);
// All 7 active slots must be CARD_SLOT_HOLE — players cannot peek at any card.
for (int i = 0; i < 7; i++)
  assert(choice->card_slot[i] == CARD_SLOT_HOLE);
// Remaining 2 slots are unused.
assert(choice->card_slot[7] == CARD_SLOT_UNUSED);
assert(choice->card_slot[8] == CARD_SLOT_UNUSED);

// --- find_game_choice_by_type ---

const GameChoice_t *found = find_game_choice_by_type(0x0A);
assert(found != NULL);
assert(found->game_type == choice->game_type);
assert(find_game_choice_by_type(0xFF) == NULL);

// --- deal_cards_to_players: all 7 slots face-down in hand, real in real_hand ---

Path_t path = {0};
get_data_dir(&path);
GameState_t game_state = {0};
CliArgs_t cli_args = {0};
init_game_state(&game_state, &path, &cli_args);

for (int i = 0; i < 3; i++) {
  game_state.player[i].is_connected = true;
  game_state.player[i].in = true;
}
game_state.dealer_id = 0;

DH_Deck deck = DH_get_new_deck();
DH_pcg_srand(42, 1);
DH_shuffle_deck(&deck);

POKEVAL_Hand_9 real_hand[MAX_PLAYERS] = {0};
deal_cards_to_players(&game_state, &deck, choice->game_type, real_hand);

// Visible hand must be all backs for the 7 active slots; real_hand must hold
// valid (non-back, non-null) cards.
for (int p = 0; p < 3; p++) {
  for (int c = 0; c < 7; c++) {
    assert(DH_is_card_back(game_state.player[p].hand.card[c]));
    assert(!DH_is_card_back(real_hand[p].card[c]));
    assert(!DH_is_card_null(real_hand[p].card[c]));
  }
  // Slots 7-8 are unused — null in both views.
  for (int c = 7; c < MAX_HAND_SIZE; c++) {
    assert(DH_is_card_null(game_state.player[p].hand.card[c]));
    assert(DH_is_card_null(real_hand[p].card[c]));
  }
}

// --- Flip-ordering comparisons (the core no-peek mechanic) ---
//
// game_seven_card_no_peek drives turns with:
//   if (POKEVAL_score_visible_cards(visible, n) > best_score) -> beat
// These assertions verify the expected outcomes for common flip sequences.

// 1. A Jack does not beat another Jack regardless of suit — the second player
//    must flip at least one more card.
DH_Card jack_h = {DH_CARD_JACK, DH_SUIT_HEARTS};
DH_Card jack_s = {DH_CARD_JACK, DH_SUIT_SPADES};
uint64_t score_jack = POKEVAL_score_visible_cards(&jack_h, 1);
assert(POKEVAL_score_visible_cards(&jack_s, 1) == score_jack);

// 2. Kicker beats a missing card: J+3 beats J alone (flip 2 does the job).
DH_Card j_and_3[2] = {{DH_CARD_JACK, DH_SUIT_HEARTS}, {DH_CARD_THREE, DH_SUIT_CLUBS}};
assert(POKEVAL_score_visible_cards(j_and_3, 2) > score_jack);

// 3. A pair beats a single high card: 9-9 beats an Ace.
DH_Card pair_nines[2] = {{DH_CARD_NINE, DH_SUIT_CLUBS}, {DH_CARD_NINE, DH_SUIT_HEARTS}};
DH_Card ace = {DH_CARD_ACE, DH_SUIT_SPADES};
assert(POKEVAL_score_visible_cards(pair_nines, 2) > POKEVAL_score_visible_cards(&ace, 1));

// 4. Simulate two flips: player B must flip past a Jack to beat player A's Jack.
//    Flip 1 (B shows J): equal — no beat, B must continue.
//    Flip 2 (B shows J+8): J+8 > J alone — beats, betting round triggered.
DH_Card b1[1] = {{DH_CARD_JACK, DH_SUIT_DIAMONDS}};
DH_Card b2[2] = {{DH_CARD_JACK, DH_SUIT_DIAMONDS}, {DH_CARD_EIGHT, DH_SUIT_SPADES}};
assert(POKEVAL_score_visible_cards(b1, 1) <= score_jack); // no beat
assert(POKEVAL_score_visible_cards(b2, 2) > score_jack);  // beats on 2nd flip

_MAIN_TAIL_
