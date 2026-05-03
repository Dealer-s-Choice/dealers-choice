#include "00_test.h"

/* Test that max_raises = 3 allows three raises in a single betting round.
 *
 * Each pass: dealer rotates (0→1→2), starting turn is left of dealer.
 * Turn order per pass (bet, raise1, raise2, raise3, call1, call2):
 *   pass 0: 1  2  0  1  2  0
 *   pass 1: 2  0  1  2  0  1
 *   pass 2: 0  1  2  0  1  2
 *
 * Raise slots: raises_remaining starts at 3; the opening BET does not
 * consume one (total_bets_plus_raises == 0 at that moment).  Three
 * subsequent RAISE actions should decrement it to 2, 1, 0.
 */

_MAIN_HEAD_
_SETUP_SOCKET_CONTEXT()

SDL_Delay(n_ms);
fprintf(stderr, "Dealer %d selecting game\n", *dealer_id);
assert(send_game_select(socket_context[*dealer_id].sock, game_choices[FIVE_CARD_SHOWDOWN].game_type,
                        false) == 0);

_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

int8_t *turn_id = &client_state[0].turn_id;

const int expected_bet_turn[3] = {1, 2, 0};
const int expected_r1_turn[3] = {2, 0, 1};
const int expected_r2_turn[3] = {0, 1, 2};
const int expected_r3_turn[3] = {1, 2, 0};
const int expected_call1_turn[3] = {2, 0, 1};
const int expected_call2_turn[3] = {0, 1, 2};

SDL_Delay(n_ms);

/* Opening bet — does NOT count against the raise cap */
assert(expected_bet_turn[game] == *turn_id);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_BET, 500) == 0);
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
assert(game_state[0].raises_remaining == 3);

/* 1st raise */
assert(expected_r1_turn[game] == *turn_id);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_RAISE, 500) == 0);
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
assert(game_state[0].raises_remaining == 2);

/* 2nd raise */
assert(expected_r2_turn[game] == *turn_id);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_RAISE, 500) == 0);
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
assert(game_state[0].raises_remaining == 1);

/* 3rd raise — the key assertion */
assert(expected_r3_turn[game] == *turn_id);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_RAISE, 500) == 0);
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
assert(game_state[0].raises_remaining == 0);

/* Both remaining players call to end the round */
assert(expected_call1_turn[game] == *turn_id);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

assert(expected_call2_turn[game] == *turn_id);
assert(send_player_action(client_state, socket_context[*turn_id].sock, ACTION_CALL, 0) == 0);
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()
_RECEIVE_GAME_STATE()

assert(game_state[0].pot == 0);

SDL_Delay(n_ms);
}

_SOCKET_CLEANUP_AND_NET_QUIT_

_MAIN_TAIL_
