#include "00_test.h"

_MAIN_HEAD_
_SETUP_SOCKET_CONTEXT()

SDL_Delay(n_ms);
fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
assert(send_game_select(socket_context[*dealer_id].sock, game_choices[FIVE_CARD_SHOWDOWN].game_type,
                        false) == 0);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

int8_t *turn_id = &game_state[0].turn_id;

SDL_Delay(n_ms);

if (game == 0) {

  assert(*turn_id == 1);
  assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  assert(*turn_id == 2);
  assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);
  SDL_Delay(n_ms);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  assert(*turn_id == 0);
  assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);
  SDL_Delay(n_ms);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  for (i = 0; i < N_PLAYERS; i++) {
    fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
  }
} else {

  assert(*turn_id == 2);
  assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);
  SDL_Delay(n_ms);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  assert(*turn_id == 0);
  assert(send_player_action(socket_context[*turn_id].sock, ACTION_BET, 500) == 0);
  SDL_Delay(n_ms);

  for (i = 0; i < N_PLAYERS; i++) {
    debug_print_cards(&game_state[i].player[i].hand);
    fputc('\n', stderr);
  }

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  assert(*turn_id == 1);
  assert(send_player_action(socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);
  SDL_Delay(n_ms);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  assert(*turn_id == 2);
  assert(send_player_action(socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);
  SDL_Delay(n_ms);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
}
_RECEIVE_GAME_STATE()

for (i = 0; i < N_PLAYERS; i++) {
  debug_print_cards(&game_state[i].player[i].hand);
  fputc('\n', stderr);
}

for (i = 0; i < N_PLAYERS; i++) {
  fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
}

fprintf(stderr, "%d\n", game_state[0].pot);
assert(game_state[0].pot == 0);

const int expected_coins[][3] = {{19950, 20100, 19950}, {19400, 21200, 19400}};
for (i = 0; i < N_PLAYERS; i++)
  assert(game_state[0].player[i].coins == expected_coins[game][i]);

if (game == 1)
  break;
SDL_Delay(n_ms);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
