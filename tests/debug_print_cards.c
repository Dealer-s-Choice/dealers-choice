#include "00_test.h"

_MAIN_HEAD_

Path_t path = {0};
get_data_dir(&path);

fprintf(stderr, "datadir: %s\n", path.data);

GameState_t game_state;
init_game_state(&game_state, &path);

for (int i = 0; i < 3; i++) {
  game_state.player[i].id = i;
}

DH_Deck deck = DH_get_new_deck();
DH_pcg_srand(1, 1);
DH_shuffle_deck(&deck);

RealHand_t real_hand =
    deal_cards_to_players(&game_state, &deck, game_choices[FIVE_CARD_DRAW].game_type);
DebugPrintCards_t cards_str = debug_print_cards(&real_hand.player[0]);

fprintf(stderr, "--%s--", cards_str.str);
assert(strcmp(cards_str.str, "4♦8♠3♠4♣2♠") == 0);

fprintf(stderr, "--%s--", cards_str.str);
cards_str = debug_print_cards(&real_hand.player[1]);
assert(strcmp(cards_str.str, "2♦5♠5♥5♣7♠") == 0);

fprintf(stderr, "--%s--", cards_str.str);
cards_str = debug_print_cards(&real_hand.player[2]);
assert(strcmp(cards_str.str, "2♥6♦3♦8♣7♦") == 0);

_MAIN_TAIL_
