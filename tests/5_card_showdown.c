#include "00_test.h"

int main(int argc, char *argv[]) {
  _SETUP_SOCKET_CONTEXT_
  sleep(n_seconds);
  fprintf(stderr, "Dealer %d selecting game\n", dealer_id);
  assert(send_game_select(socket_context[dealer_id].sock,
                          game_choices[FIVE_CARD_SHOWDOWN].game_type) == 0);

  sleep(n_seconds);
  for (i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
    assert(socket_context[i].sock != NULL);
  }

  sleep(n_seconds);

  assert(send_player_action(socket_context[game_state[0].turn_id].sock, ACTION_BET, 500) == 0);

  for (i = 0; i < 2; i++) {
    debug_print_cards(&game_state[i].player[i].hand);
    fputc('\n', stderr);
  }

  for (int recv = 0; recv < 2; recv++) {
    sleep(n_seconds);
    for (i = 0; i < 2; i++) {
      assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                             &client_state[i], socket_context[i].id) != RECV_ERROR);
      assert(socket_context[i].sock != NULL);
    }
  }

  sleep(n_seconds);
  assert(send_player_action(socket_context[game_state[0].turn_id].sock, ACTION_CALL, 0) == 0);

  for (int recv = 0; recv < 4; recv++) {
    sleep(n_seconds);
    for (i = 0; i < 2; i++) {
      assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                             &client_state[i], socket_context[i].id) != RECV_ERROR);
      assert(socket_context[i].sock != NULL);
    }
  }

  sleep(n_seconds);
  for (i = 0; i < 2; i++) {
    fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
  }
  fprintf(stderr, "%d\n", game_state[0].pot);

  switch (game) {
  case 0:
    assert(game_state[0].player[0].coins == 19250);
    assert(game_state[0].player[1].coins == 20750);
    break;
  case 1:
    assert(game_state[0].player[0].coins == 20000);
    assert(game_state[0].player[1].coins == 20000);
    break;
  case 2:
    assert(game_state[0].player[0].coins == 20750);
    assert(game_state[0].player[1].coins == 19250);
    break;
  }
  sleep(n_seconds);
}

sleep(1);

for (int i = 0; i < 2; i++) {
  SDLNet_TCP_DelSocket(socket_context[i].set, socket_context[i].sock);
  SDLNet_FreeSocketSet(socket_context[i].set);
  SDLNet_TCP_Close(socket_context[i].sock);
  SDLNet_Quit();
}

return 0;
}
