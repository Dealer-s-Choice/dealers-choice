#include "00_test.h"

int main(int argc, char *argv[]) {
  _SETUP_SOCKET_CONTEXT()
  usleep(n_useconds);
  fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
  assert(send_game_select(socket_context[*dealer_id].sock,
                          game_choices[FIVE_CARD_STUD].game_type) == 0);

  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()
  _RECEIVE_GAME_STATE()

  for (int n_rounds = 0; n_rounds < game_choices[FIVE_CARD_STUD].n_betting_rounds; n_rounds++) {
    fprintf(stderr, "\n -#- game: %d -#- n_rounds: %d\n", game, n_rounds);

    int8_t *turn_id = &game_state[0].turn_id;

    const int expected_bet_turn[3] = {1, 0, 1};
    assert(expected_bet_turn[game] == *turn_id);

    usleep(n_useconds);
    fprintf(stderr, "turn_id: %d sending bet...\n", *turn_id);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_BET, 500) == 0);

    for (i = 0; i < 2; i++) {
      debug_print_cards(&game_state[i].player[i].hand);
      fputc('\n', stderr);
    }

    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()

    const int expected_call_turn[3] = {0, 1, 0};
    assert(expected_call_turn[game] == *turn_id);

    usleep(n_useconds);
    fprintf(stderr, "turn_id: %d\n", *turn_id);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);

    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
    _RECEIVE_GAME_STATE()
  }

  _RECEIVE_GAME_STATE()

  usleep(n_useconds);
  for (i = 0; i < 2; i++)
    fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);

  fprintf(stderr, "%d\n", game_state[0].pot);

  const int expected_coins[3][2] = {{22250, 17750}, {20000, 20000}, {17750, 22250}};
  assert(game_state[0].player[0].coins == expected_coins[game][0]);
  assert(game_state[0].player[1].coins == expected_coins[game][1]);

  usleep(n_useconds);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

return 0;
}
