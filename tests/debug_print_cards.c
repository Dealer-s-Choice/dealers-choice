#include "00_test.h"

_MAIN_HEAD_

Game_State game_state;
init_game_state(&game_state);

for (int i = 0; i < 3; i++) {
  game_state.player[i].id = i;
}

struct dh_deck deck = dh_get_new_deck();
dh_pcg_srand(1, 1);
dh_shuffle_deck(&deck);

struct player_t *dealer = &game_state.player[0];
RealHand real_hand =
    deal_cards_to_players(&game_state, dealer, &deck, game_choices[FIVE_CARD_DRAW].game_type);
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
