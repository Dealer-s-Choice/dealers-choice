#include "00_test.h"

_MAIN_HEAD_
_SETUP_SOCKET_CONTEXT()
SDL_Delay(n_ms);
fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
assert(send_game_select(socket_context[*dealer_id].sock, game_choices[FIVE_CARD_STUD].game_type,
                        false) == 0);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

for (int n_rounds = 0; n_rounds < game_choices[FIVE_CARD_STUD].n_betting_rounds; n_rounds++) {
  fprintf(stderr, "\n -#- game: %d -#- n_rounds: %d\n", game, n_rounds);

  int8_t *turn_id = &client_state[0].turn_id;

  const int expected_bet_turn[] = {1, 2, 0};
  assert(expected_bet_turn[game] == *turn_id);

  SDL_Delay(n_ms);
  fprintf(stderr, "turn_id: %d sending bet...\n", *turn_id);
  assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_BET, 500) == 0);

  for (i = 0; i < N_PLAYERS; i++) {
    debug_print_cards(&game_state[i].player[i].hand);
    fputc('\n', stderr);
  }

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  const int expected_call_turn[3] = {2, 0, 1};
  assert(expected_call_turn[game] == *turn_id);

  SDL_Delay(n_ms);
  fprintf(stderr, "turn_id: %d\n", *turn_id);
  assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  // const int expected_call_turn[3] = {2, 0, 1};
  // assert(expected_call_turn[game] == *turn_id);

  SDL_Delay(n_ms);
  fprintf(stderr, "turn_id: %d\n", *turn_id);
  assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
}

_RECEIVE_GAME_STATE()

SDL_Delay(n_ms);
for (i = 0; i < N_PLAYERS; i++)
  fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);

fprintf(stderr, "%d\n", game_state[0].pot);
assert(game_state[0].pot == 0);

const int expected_coins[][3] = {
    {17950, 17950, 24100}, {15900, 15900, 28200}, {20000, 13850, 26150}};

for (i = 0; i < N_PLAYERS; i++)
  assert(game_state[0].player[i].coins == expected_coins[game][i]);

SDL_Delay(n_ms);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
