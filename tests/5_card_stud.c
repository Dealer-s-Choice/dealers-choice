#include "00_test.h"

_MAIN_HEAD_
_SETUP_SOCKET_CONTEXT()
SDL_Delay(n_ms);
fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
assert(send_game_select(socket_context[*dealer_id].sock, game_choices[FIVE_CARD_STUD].game_type,
                        false) == 0);

// Initial deal + game-type broadcast + bring-in status msg + bring-in game state + first turn-id
#include "01_recv_game_state_5x.c"

for (int n_rounds = 0; n_rounds < game_choices[FIVE_CARD_STUD].n_betting_rounds; n_rounds++) {
  fprintf(stderr, "\n -#- game: %d -#- n_rounds: %d\n", game, n_rounds);

  int8_t *turn_id = &client_state[0].turn_id;

  SDL_Delay(n_ms);
  if (n_rounds == 0) {
    // Bring-in round: server opened with a forced partial bet, so all players call.
    fprintf(stderr, "turn_id: %d sending call (bring-in round)...\n", *turn_id);
    assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);
  } else {
    fprintf(stderr, "turn_id: %d sending bet...\n", *turn_id);
    assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_BET, 500) == 0);
  }

  for (i = 0; i < N_PLAYERS; i++) {
    debug_print_cards(&game_state[i].player[i].hand);
    fputc('\n', stderr);
  }

#include "01_recv_game_state_4x.c"

  SDL_Delay(n_ms);
  fprintf(stderr, "turn_id: %d\n", *turn_id);
  assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);

#include "01_recv_game_state_4x.c"

  SDL_Delay(n_ms);
  fprintf(stderr, "turn_id: %d\n", *turn_id);
  // In the bring-in round the bring-in player already paid; they owe nothing on
  // their second turn and receive BET_CHECK_FOLD, so send CHECK instead of CALL.
  uint8_t last_action = (n_rounds == 0) ? ACTION_CHECK : ACTION_CALL;
  assert(send_player_action(client_state, socket_context[*turn_id].sock, last_action, 0) == 0);

#include "01_recv_game_state_4x.c"
}

_RECEIVE_GAME_STATE()

SDL_Delay(n_ms);
for (i = 0; i < N_PLAYERS; i++)
  fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);

fprintf(stderr, "pot: %d\n", game_state[0].pot);
assert(game_state[0].pot == 0);

int total_coins = 0;
for (i = 0; i < N_PLAYERS; i++)
  total_coins += game_state[0].player[i].coins;
assert(total_coins == N_PLAYERS * STARTING_N_COINS);

SDL_Delay(n_ms);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
