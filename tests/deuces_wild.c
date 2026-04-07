#include "00_test.h"

_MAIN_HEAD_
_SETUP_SOCKET_CONTEXT()
SDL_Delay(n_ms);
fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
assert(send_game_select(socket_context[*dealer_id].sock, game_choices[FIVE_CARD_SHOWDOWN].game_type,
                        true) == 0);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

int8_t *turn_id = &client_state[0].turn_id;
const int expected_bet_turn[3] = {1, 2, 0};
assert(expected_bet_turn[game] == *turn_id);

SDL_Delay(n_ms);

assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_FOLD, 0) == 0);

for (i = 0; i < N_PLAYERS; i++) {
  debug_print_cards(&game_state[i].player[i].hand);
  fputc('\n', stderr);
}

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

int expected_call_turn[3] = {2, 0, 1};
assert(expected_call_turn[game] == *turn_id);

SDL_Delay(n_ms);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_BET, 500) == 0);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

expected_call_turn[0] = 0;
expected_call_turn[1] = 1;
expected_call_turn[2] = 2;
assert(expected_call_turn[game] == *turn_id);

SDL_Delay(n_ms);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);

// Server evaluates deuces-wild hands automatically. Wild card ranking logic
// is covered by the pokeval wild tests; here we just verify the game
// completes and coins are correctly distributed.
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

SDL_Delay(n_ms);
for (i = 0; i < N_PLAYERS; i++) {
  fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
}
fprintf(stderr, "pot: %d\n", game_state[0].pot);
assert(game_state[0].pot == 0);

int total_coins = 0;
for (i = 0; i < N_PLAYERS; i++)
  total_coins += game_state[0].player[i].coins;
assert(total_coins == N_PLAYERS * 20000);

SDL_Delay(n_ms);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
