#include "00_test.h"

_MAIN_HEAD_
_SETUP_SOCKET_CONTEXT()
SDL_Delay(n_ms);
fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
assert(send_game_select(socket_context[*dealer_id].sock,
                        game_choices[FIVE_CARD_TRIPLE_DRAW].game_type, false) == 0);

#include "01_recv_game_state_3x.c"

int8_t *turn_id = &client_state[0].turn_id;
int expected_turn[3] = {1, 2, 0};

uint8_t discard_indices[MAX_HAND_SIZE] = {0, 1, 2, 3, 4};

/* Three draw rounds: each preceded by a betting round in which every player
 * checks.  Round 1 draws 3 cards, round 2 draws 2, round 3 draws 1. */
const uint8_t draw_counts[3] = {3, 2, 1};

for (int dr = 0; dr < 3; dr++) {
  /* betting round — all three players check */
  for (int n = 0; n < N_PLAYERS; n++) {
    assert(expected_turn[n] == *turn_id);
    fprintf(stderr, "CHECK round %d, turn %d\n", dr, *turn_id);
    assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);
    SDL_Delay(n_ms);
#include "01_recv_game_state_4x.c"
  }

  /* draw round — each player discards draw_counts[dr] cards */
  for (int n = 0; n < N_PLAYERS; n++) {
    assert(expected_turn[n] == *turn_id);
    fprintf(stderr, "DRAW round %d (%u cards), turn %d\n", dr, draw_counts[dr], *turn_id);
    send_discards_request_new_cards(socket_context[*turn_id].sock, discard_indices,
                                    draw_counts[dr]);
    SDL_Delay(n_ms);
#include "01_recv_game_state_5x.c"
  }
}

/* final betting round — all check, then showdown */
for (int n = 0; n < N_PLAYERS; n++) {
  assert(expected_turn[n] == *turn_id);
  fprintf(stderr, "FINAL CHECK turn %d\n", *turn_id);
  assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CHECK, 0) == 0);
  SDL_Delay(n_ms);
#include "01_recv_game_state_4x.c"
}

#include "01_recv_game_state_3x.c"

for (i = 0; i < N_PLAYERS; i++)
  fprintf(stderr, "%d: %d\n", i, game_state[i].player[i].coins);
fprintf(stderr, "pot: %d\n", game_state[0].pot);
assert(game_state[0].pot == 0);

break;
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
