#include "00_test.h"

int main(int argc, char *argv[]) {
#include "_setup_socket_context.c"

  sleep(n_seconds);
  fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
  assert(send_game_select(socket_context[*dealer_id].sock,
                          game_choices[FIVE_CARD_SHOWDOWN].game_type) == 0);

#include "_receive_game_state.c"

  int8_t *turn_id = &game_state[0].turn_id;
  sleep(n_seconds);

  if (game == 0) {

    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);

#include "_receive_game_state.c"
#include "_receive_game_state.c"

    sleep(n_seconds);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);

#include "_receive_game_state.c"
#include "_receive_game_state.c"

    sleep(n_seconds);
    for (i = 0; i < 2; i++) {
      fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
    }
  } else {

    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);

#include "_receive_game_state.c"
#include "_receive_game_state.c"

    // const int expected_bet_turn[3] = {0, 1, 0};
    // assert(expected_bet_turn[game] == *turn_id);

    assert(1 == *turn_id);
    sleep(n_seconds);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_BET, 500) == 0);
    for (i = 0; i < 2; i++) {
      debug_print_cards(&game_state[i].player[i].hand);
      fputc('\n', stderr);
    }

#include "_receive_game_state.c"
#include "_receive_game_state.c"

    // const int expected_call_turn[3] = {0, 1, 0};
    // assert(expected_call_turn[game] == *turn_id);

    assert(0 == *turn_id);
    sleep(n_seconds);
    assert(send_player_action(socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);

#include "_receive_game_state.c"
#include "_receive_game_state.c"
  }
#include "_receive_game_state.c"

  for (i = 0; i < 2; i++) {
    debug_print_cards(&game_state[i].player[i].hand);
    fputc('\n', stderr);
  }

  sleep(n_seconds);
  for (i = 0; i < 2; i++) {
    fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
  }

  fprintf(stderr, "%d\n", game_state[0].pot);

  const int expected_coins[3][2] = {{19750, 20250}, {20500, 19500}, {20750, 19250}};
  assert(game_state[0].player[0].coins == expected_coins[game][0]);
  assert(game_state[0].player[1].coins == expected_coins[game][1]);

  if (game == 1)
    break;
  sleep(n_seconds);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

return 0;
}
