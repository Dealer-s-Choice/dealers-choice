#include "00_test.h"

_MAIN_HEAD_

Path_t path = {0};
get_data_dir(&path);

fprintf(stderr, "datadir: %s\n", path.data);

GameState_t game_state;

CliArgs_t cli_args = {0};
// test_mode isn't needed here; it isn't used or required for all tests
cli_args.test_mode = false;
init_game_state(&game_state, &path, &cli_args);

for (int i = 0; i < 3; i++) {
  game_state.player[i].is_connected = true;
  game_state.player[i].in = true;
}

DH_Deck deck = DH_get_new_deck();
DH_pcg_srand(1, 1);
DH_shuffle_deck(&deck);

POKEVAL_Hand_9 real_hand[MAX_PLAYERS] = {0};
deal_cards_to_players(&game_state, &deck, game_choices[FIVE_CARD_DRAW].game_type, real_hand);
DebugPrintCards_t cards_str = debug_print_cards(&real_hand[0]);

fprintf(stderr, "--%s--", cards_str.str);
assert(strcmp(cards_str.str, "5♥2♥8♣8♠2♠") == 0);

fprintf(stderr, "--%s--", cards_str.str);
cards_str = debug_print_cards(&real_hand[1]);
assert(strcmp(cards_str.str, "2♦5♣6♦7♦3♠") == 0);

fprintf(stderr, "--%s--", cards_str.str);
cards_str = debug_print_cards(&real_hand[2]);
assert(strcmp(cards_str.str, "5♠7♠3♦4♦4♣") == 0);

_MAIN_TAIL_
