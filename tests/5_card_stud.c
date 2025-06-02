#include "00_test.h"

int main(int argc, char *argv[]) {
  int n_passes = 3;

  char *ptr_n_passes = getenv("DC_NPASSES");
  if (ptr_n_passes)
    n_passes = atoi(ptr_n_passes);
  else if (argc > 1)
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
    sleep(1);
    assert(socket_context[i].sock != NULL);
  }

  for (int game = 0; game < n_passes; game++) {
    fprintf(stderr, "\n-#- game: %d\n", game);
    const int n_seconds = 1;
    sleep(n_seconds);
    int i;

    for (int recv = 0; recv < 2; recv++) {
      sleep(n_seconds);
      for (i = 0; i < 2; i++) {
        assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                               &client_state[i], socket_context[i].id) != RECV_ERROR);
        assert(socket_context[i].sock != NULL);
      }
    }

    const int dealer_id = game_state[0].dealer_id;
    switch (game) {
    case 0:
      assert(dealer_id == 0);
      break;
    case 1:
      assert(dealer_id == 1);
      break;
    case 2:
      assert(dealer_id == 0);
      break;
    }

    sleep(n_seconds);
    fprintf(stderr, "Dealer %d selecting game\n", dealer_id);
    assert(send_game_select(socket_context[game_state[0].dealer_id].sock,
                            game_choices[FIVE_CARD_STUD].game_type) == 0);

    sleep(n_seconds);
    for (i = 0; i < 2; i++) {
      assert(recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i],
                             &client_state[i], socket_context[i].id) != RECV_ERROR);
    }

    for (int n_rounds = 0; n_rounds < game_choices[FIVE_CARD_STUD].n_betting_rounds; n_rounds++) {
      fprintf(stderr, "\n -#- game: %d -#- n_rounds: %d\n", game, n_rounds);
      sleep(n_seconds);

      fprintf(stderr, "turn_id: %d sending bet...\n", game_state[0].turn_id);
      assert(send_player_action(socket_context[game_state[0].turn_id].sock, ACTION_BET, 500) == 0);

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

      sleep(n_seconds);

      fprintf(stderr, "turn_id: %d\n", game_state[0].turn_id);
      assert(send_player_action(socket_context[game_state[0].turn_id].sock, ACTION_CALL, 0) == 0);

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

    sleep(n_seconds);

    switch (game) {
    case 0:
      assert(game_state[0].player[0].coins == 22000);
      assert(game_state[0].player[1].coins == 18000);
      break;
    case 1:
      assert(game_state[0].player[0].coins == 20000);
      assert(game_state[0].player[1].coins == 20000);
      break;
    case 2:
      assert(game_state[0].player[0].coins == 18000);
      assert(game_state[0].player[1].coins == 22000);
      break;
    }
    sleep(n_seconds);
  }

  sleep(2);

  for (int i = 0; i < 2; i++) {
    SDLNet_TCP_DelSocket(socket_context[i].set, socket_context[i].sock);
    SDLNet_FreeSocketSet(socket_context[i].set);
    SDLNet_TCP_Close(socket_context[i].sock);
    SDLNet_Quit();
  }

  return 0;
}
