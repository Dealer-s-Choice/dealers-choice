#include "00_test.h"

_MAIN_HEAD_
_SETUP_SOCKET_CONTEXT()

SDL_Delay(n_ms);
fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
assert(send_game_select(socket_context[*dealer_id].sock, game_choices[TEXAS_HOLDEM].game_type,
                        false) == 0);

/* Receive initial 2-card hole card deal */
#include "01_recv_game_state_3x.c"

int8_t *turn_id = &client_state[0].turn_id;

/* Pre-flop betting */
const int preflop_order[2][3] = {{1, 2, 0}, {2, 0, 1}};
for (int p = 0; p < N_PLAYERS; p++) {
  fprintf(stderr, "pre-flop turn: %d (expected %d)\n", *turn_id, preflop_order[game][p]);
  assert(*turn_id == preflop_order[game][p]);
  assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);
  SDL_Delay(n_ms);
#include "01_recv_game_state_4x.c"
}

/* Flop: 3 community cards dealt */
#include "01_recv_game_state_3x.c"

for (int p = 0; p < N_PLAYERS; p++) {
  fprintf(stderr, "flop turn: %d (expected %d)\n", *turn_id, preflop_order[game][p]);
  assert(*turn_id == preflop_order[game][p]);
  assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);
  SDL_Delay(n_ms);
#include "01_recv_game_state_4x.c"
}

/* Turn: 1 community card */
#include "01_recv_game_state_3x.c"

for (int p = 0; p < N_PLAYERS; p++) {
  fprintf(stderr, "turn street turn: %d (expected %d)\n", *turn_id, preflop_order[game][p]);
  assert(*turn_id == preflop_order[game][p]);
  assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);
  SDL_Delay(n_ms);
#include "01_recv_game_state_4x.c"
}

/* River: 1 community card */
#include "01_recv_game_state_3x.c"

for (int p = 0; p < N_PLAYERS; p++) {
  fprintf(stderr, "river turn: %d (expected %d)\n", *turn_id, preflop_order[game][p]);
  assert(*turn_id == preflop_order[game][p]);
  assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);
  SDL_Delay(n_ms);
#include "01_recv_game_state_4x.c"
}

/* Showdown */
#include "01_recv_game_state_3x.c"

for (i = 0; i < N_PLAYERS; i++) {
  debug_print_cards(&game_state[i].player[i].hand);
  fputc('\n', stderr);
}

for (i = 0; i < N_PLAYERS; i++)
  fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);

fprintf(stderr, "pot: %d\n", game_state[0].pot);
assert(game_state[0].pot == 0);

const int expected_coins[][3] = {{19950, 20100, 19950}, {19900, 20050, 20050}};
for (i = 0; i < N_PLAYERS; i++)
  assert(game_state[0].player[i].coins == expected_coins[game][i]);

if (game == 1)
  break;
SDL_Delay(n_ms);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
