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

// Deuces wild exchange phase: server broadcasts player_exchanging=true, then for each
// remaining player that has a 2 it sends MSG_WILD_REPLACEMENT (sets do_exchange_wilds).
// Respond with an empty hand (no replacements), leaving the 2s as-is.
// 10 rounds covers up to 2 exchanges plus the normal post-round broadcasts.
for (int xr = 0; xr < 10; xr++) {
  _RECEIVE_GAME_STATE()
  for (int xj = 0; xj < N_PLAYERS; xj++) {
    if (client_state[xj].do_exchange_wilds) {
      fprintf(stderr, "Player %d: sending empty wild exchange\n", xj);
      POKEVAL_Hand_7 empty_hand = {0};
      size_t xsz = 0;
      uint8_t *xdata = serialize_hand(empty_hand, &xsz);
      assert(xdata != NULL);
      assert(send_all_tcp(socket_context[xj].sock, xdata, xsz) == 0);
      free(xdata);
      client_state[xj].do_exchange_wilds = false;
    }
  }
}

SDL_Delay(n_ms);
for (i = 0; i < N_PLAYERS; i++) {
  fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
}
fprintf(stderr, "%d\n", game_state[0].pot);
assert(game_state[0].pot == 0);

// With empty wild exchanges the hands are unchanged; results match 5_card_showdown
const int expected_coins[3][3] = {
    {19450, 19950, 20600}, {20050, 19400, 20550}, {20000, 19425, 20575}};

for (i = 0; i < N_PLAYERS; i++)
  assert(game_state[0].player[i].coins == expected_coins[game][i]);

SDL_Delay(n_ms);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
