#include "00_test.h"

_MAIN_HEAD_
_SETUP_SOCKET_CONTEXT()
SDL_Delay(n_ms);
fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
assert(send_game_select(socket_context[*dealer_id].sock, game_choices[FIVE_CARD_DRAW].game_type,
                        false) == 0);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

int8_t *turn_id = &client_state[0].turn_id;
const int expected_bet_turn[3] = {1, 2, 0};
assert(expected_bet_turn[game] == *turn_id);

SDL_Delay(n_ms);

assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_BET, 500) == 0);

for (i = 0; i < N_PLAYERS; i++) {
  debug_print_cards(&game_state[i].player[i].hand);
  fputc('\n', stderr);
}

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

int expected_turn[3] = {2, 0, 1};
assert(expected_turn[game] == *turn_id);
SDL_Delay(n_ms);

assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_FOLD, 0) == 0);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

expected_turn[0] = 0;
expected_turn[1] = 1;
expected_turn[2] = 2;
assert(expected_turn[game] == *turn_id);
SDL_Delay(n_ms);

assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);
SDL_Delay(n_ms);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

uint8_t discard_indices[MAX_HAND_SIZE] = {0};
const uint8_t discard_count = 2;
discard_indices[0] = 0;
discard_indices[1] = 3;

expected_turn[0] = 0;
expected_turn[1] = 1;
expected_turn[2] = 2;
assert(expected_turn[game] == *turn_id);

send_discards_request_new_cards(socket_context[*turn_id].sock, discard_indices, discard_count);
SDL_Delay(n_ms);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

expected_turn[0] = 1;
expected_turn[1] = 2;
expected_turn[2] = 0;
assert(expected_turn[game] == *turn_id);
SDL_Delay(n_ms);

send_discards_request_new_cards(socket_context[*turn_id].sock, discard_indices, discard_count);
SDL_Delay(n_ms);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

expected_turn[0] = 0;
expected_turn[1] = 1;
expected_turn[2] = 2;
assert(expected_turn[game] == *turn_id);
SDL_Delay(n_ms);

assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_BET, 500) == 0);
SDL_Delay(n_ms);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

expected_turn[0] = 1;
expected_turn[1] = 2;
expected_turn[2] = 0;
assert(expected_turn[game] == *turn_id);
SDL_Delay(n_ms);

assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_RAISE, 500) == 0);
SDL_Delay(n_ms);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

expected_turn[0] = 0;
expected_turn[1] = 1;
expected_turn[2] = 2;
assert(expected_turn[game] == *turn_id);
SDL_Delay(n_ms);

assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);
SDL_Delay(n_ms);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

for (i = 0; i < N_PLAYERS; i++) {
  fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
}
fprintf(stderr, "%d\n", game_state[0].pot);
assert(game_state[0].pot == 0);

for (i = 0; i < N_PLAYERS; i++) {
  debug_print_cards(&game_state[i].player[i].hand);
  fputc('\n', stderr);
}

const int expected_coins[][3] = {
    {18450, 21600, 19950}, {18400, 20050, 21550}, {20000, 20000, 20000}};

for (i = 0; i < N_PLAYERS; i++)
  assert(game_state[0].player[i].coins == expected_coins[game][i]);

SDL_Delay(n_ms);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
