#include "00_test.h"

_MAIN_HEAD_
_SETUP_SOCKET_CONTEXT()
SDL_Delay(n_ms);
fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
assert(send_game_select(socket_context[*dealer_id].sock, game_choices[FIVE_CARD_SHOWDOWN].game_type,
                        false) == 0);

#include "01_recv_game_state_4x.c"

int8_t *turn_id = &client_state[0].turn_id;
const int expected_bet_turn[3] = {1, 2, 0};
assert(expected_bet_turn[game] == *turn_id);

SDL_Delay(n_ms);

assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_FOLD, 0) == 0);

for (i = 0; i < N_PLAYERS; i++) {
  debug_print_cards(&game_state[i].player[i].hand);
  fputc('\n', stderr);
}

#include "01_recv_game_state_4x.c"

int expected_call_turn[3] = {2, 0, 1};
assert(expected_call_turn[game] == *turn_id);

SDL_Delay(n_ms);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_BET, 500) == 0);

#include "01_recv_game_state_4x.c"

expected_call_turn[0] = 0;
expected_call_turn[1] = 1;
expected_call_turn[2] = 2;
assert(expected_call_turn[game] == *turn_id);

SDL_Delay(n_ms);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);

#include "01_recv_game_state_5x.c"

SDL_Delay(n_ms);
for (i = 0; i < N_PLAYERS; i++) {
  fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
}
fprintf(stderr, "%d\n", game_state[0].pot);

const int expected_coins[3][3] = {
    {19450, 19950, 20600}, {20050, 19400, 20550}, {20000, 19425, 20575}};

for (i = 0; i < N_PLAYERS; i++)
  assert(game_state[0].player[i].coins == expected_coins[game][i]);

SDL_Delay(n_ms);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
