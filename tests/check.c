#include "00_test.h"

int main(int argc, char *argv[]) {
  _SETUP_SOCKET_CONTEXT()

  usleep(n_useconds);
  fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
  assert(send_game_select(socket_context[*dealer_id].sock,
                          game_choices[FIVE_CARD_SHOWDOWN].game_type) == 0);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  int8_t *turn_id = &game_state[0].turn_id;
  usleep(n_useconds);

  if (game == 0) {

    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);

    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()

    usleep(n_useconds);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);

    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()

    usleep(n_useconds);
    for (i = 0; i < 2; i++) {
      fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
    }
  } else {

    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);

    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()

    // const int expected_bet_turn[3] = {0, 1, 0};
    // assert(expected_bet_turn[game] == *turn_id);

    assert(1 == *turn_id);
    usleep(n_useconds);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_BET, 500) == 0);
    for (i = 0; i < 2; i++) {
      debug_print_cards(&game_state[i].player[i].hand);
      fputc('\n', stderr);
    }

    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()

    // const int expected_call_turn[3] = {0, 1, 0};
    // assert(expected_call_turn[game] == *turn_id);

    assert(0 == *turn_id);
    usleep(n_useconds);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);

    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
  }
  _RECEIVE_GAME_STATE()

  for (i = 0; i < 2; i++) {
    debug_print_cards(&game_state[i].player[i].hand);
    fputc('\n', stderr);
  }

  usleep(n_useconds);
  for (i = 0; i < 2; i++) {
    fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
  }

  fprintf(stderr, "%d\n", game_state[0].pot);

  const int expected_coins[3][2] = {{19750, 20250}, {20500, 19500}, {20750, 19250}};
  assert(game_state[0].player[0].coins == expected_coins[game][0]);
  assert(game_state[0].player[1].coins == expected_coins[game][1]);

  if (game == 1)
    break;
  usleep(n_useconds);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

return 0;
}
