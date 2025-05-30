#include "00_test.h"

_MAIN_HEAD_

SdlContext_t sdl_context = {0};
Font_t font = {0};

GameState_t game_state[2] = {0};
ClientState_t client_state = {0};
const bool test_mode = true;
char addr[] = "127.0.0.1";
SocketContext_t socket_context[2];
for (int i = 0; i < 2; i++) {
  socket_context[i] = run_client(addr, &sdl_context, &font, test_mode);
  assert(socket_context[i].sock != NULL);
}

sleep(0.5);

for (int i = 0; i < 2; i++) {
  recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i], &client_state,
                  socket_context[i].id);
  // assert(socket_context[i].sock != NULL);
}

sleep(1);

if (send_game_select(socket_context[0].sock, game_choices[FIVE_CARD_SHOWDOWN].game_type) == 0) {
  fprintf(stderr, "Game type sent: %s", game_choices[FIVE_CARD_SHOWDOWN].str);
}

sleep(1);

for (int i = 0; i < 2; i++) {
  recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i], &client_state,
                  socket_context[i].id);
  // assert(socket_context[i].sock != NULL);
}

sleep(3);

if (send_player_action(socket_context[1].sock, ACTION_BET, 500) != 0)
  fprintf(stderr, "Failed to send bet\n");

for (int i = 0; i < 2; i++) {
  debug_print_cards(&game_state[i].player[i].hand);
  fputc('\n', stderr);
}

sleep(3);

for (int i = 0; i < 2; i++) {
  recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i], &client_state,
                  socket_context[i].id);
  // assert(socket_context[i].sock != NULL);
}

sleep(3);

if (send_player_action(socket_context[0].sock, ACTION_CALL, 0) != 0)
  fprintf(stderr, "Failed to send bet\n");

sleep(4);

for (int i = 0; i < 2; i++) {
  recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i], &client_state,
                  socket_context[i].id);
  fprintf(stderr, "%d\n", game_state[i].player[i].coins);
  fprintf(stderr, "%d\n", game_state[i].pot);
  // assert(socket_context[i].sock != NULL);
}

sleep(3);

for (int i = 0; i < 2; i++) {
  recv_game_state(socket_context[i].sock, socket_context[i].set, &game_state[i], &client_state,
                  socket_context[i].id);
  fprintf(stderr, "%d\n", game_state[i].player[i].coins);
  fprintf(stderr, "%d\n", game_state[i].pot);
  // assert(socket_context[i].sock != NULL);
}

sleep(2);

for (int i = 0; i < 2; i++) {
  SDLNet_TCP_DelSocket(socket_context[i].set, socket_context[i].sock);
  SDLNet_FreeSocketSet(socket_context[i].set);
  SDLNet_TCP_Close(socket_context[i].sock);
  SDLNet_Quit();
}

return 0;

_MAIN_TAIL_
