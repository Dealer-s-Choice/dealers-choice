#include "00_test.h"

int main(int argc, char *argv[]) {

int n_passes = 3;
if (argc > 1)
  n_passes = atoi(argv[1]);

SdlContext_t sdl_context = {0};
Font_t font = {0};

GameState_t game_state[2] = {0};
ClientState_t client_state[2] = {0};
const bool test_mode = true;
char addr[] = "127.0.0.1";
SocketContext_t socket_context[2];

// There is a check for server readiness in the script that launches this
// when using 'meson test ...'
// sleep(7);

for (int i = 0; i < 2; i++) {
  socket_context[i] = run_client(addr, &sdl_context, &font, test_mode);
  assert(socket_context[i].sock != NULL);
}

sleep(1);

for (int game = 0; game < n_passes; game++) {
  for (int i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
    // assert(socket_context[i].sock != NULL);
  }

  sleep(1);

  fprintf(stderr, "Dealer %d selecting game\n", game_state[0].dealer_id);
  assert(send_game_select(socket_context[game_state[0].dealer_id].sock,
                          game_choices[FIVE_CARD_SHOWDOWN].game_type) == 0);
  fprintf(stderr, "Game type sent: %s", game_choices[FIVE_CARD_SHOWDOWN].str);

  sleep(1);

  for (int i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
    // assert(socket_context[i].sock != NULL);
  }

  sleep(1);

  assert(send_player_action(socket_context[game_state[0].turn_id].sock, ACTION_BET, 500) == 0);

  for (int i = 0; i < 2; i++) {
    debug_print_cards(&game_state[i].player[i].hand);
    fputc('\n', stderr);
  }

  sleep(1);

  // At this point (after the player action), the server should send the status message
  for (int i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
  }

  // Then the game state struct
  for (int i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
  }

  sleep(1);

  assert(send_player_action(socket_context[game_state[0].turn_id].sock, ACTION_CALL, 0) == 0);

  sleep(1);

  // recv status msg
  for (int i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
    fprintf(stderr, "%d\n", game_state[i].player[i].coins);
    fprintf(stderr, "%d\n", game_state[i].pot);
  }

  // recv another status message indicating winner
  for (int i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
    fprintf(stderr, "%d\n", game_state[i].player[i].coins);
    fprintf(stderr, "%d\n", game_state[i].pot);
  }

  // recv game state struct
  for (int i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
    fprintf(stderr, "%d\n", game_state[i].player[i].coins);
    fprintf(stderr, "%d\n", game_state[i].pot);
  }

  sleep(1);

  for (int i = 0; i < 2; i++) {
    assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                           &client_state[i], socket_context[i].id) != RECV_ERROR);
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
