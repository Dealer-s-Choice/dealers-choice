#include "00_test.h"

int main(int argc, char *argv[]) {
  _SETUP_SOCKET_CONTEXT_

  sleep(n_seconds);
  fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
  assert(send_game_select(socket_context[*dealer_id].sock,
                          game_choices[FIVE_CARD_STUD].game_type) == 0);

  sleep(n_seconds);
  for (i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
  }

  for (int n_rounds = 0; n_rounds < game_choices[FIVE_CARD_STUD].n_betting_rounds; n_rounds++) {
    fprintf(stderr, "\n -#- game: %d -#- n_rounds: %d\n", game, n_rounds);

    int8_t *turn_id = &game_state[0].turn_id;

    const int expected_bet_turn[3] = {1, 0, 1};
    assert(expected_bet_turn[game] == *turn_id);

    sleep(n_seconds);
    fprintf(stderr, "turn_id: %d sending bet...\n", *turn_id);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_BET, 500) == 0);

    for (i = 0; i < 2; i++) {
      debug_print_cards(&game_state[i].player[i].hand);
      fputc('\n', stderr);
    }

    for (int recv = 0; recv < 3; recv++) {
      sleep(n_seconds);
      for (i = 0; i < 2; i++) {
        assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                               &client_state[i], socket_context[i].id) != RECV_ERROR);
        assert(socket_context[i].sock != NULL);
      }
    }

    const int expected_call_turn[3] = {0, 1, 0};
    assert(expected_call_turn[game] == *turn_id);

    sleep(n_seconds);
    fprintf(stderr, "turn_id: %d\n", *turn_id);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);

    for (int recv = 0; recv < 3; recv++) {
      sleep(n_seconds);
      for (i = 0; i < 2; i++) {
        assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                               &client_state[i], socket_context[i].id) != RECV_ERROR);
      }
    }
  }

  sleep(n_seconds);
  for (i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
    fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
    assert(socket_context[i].sock != NULL);
  }
  fprintf(stderr, "%d\n", game_state[0].pot);

  const int expected_coins[3][2] = {{22000, 18000}, {20000, 20000}, {18000, 22000}};
  assert(game_state[0].player[0].coins == expected_coins[game][0]);
  assert(game_state[0].player[1].coins == expected_coins[game][1]);

  sleep(n_seconds);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

return 0;
}
