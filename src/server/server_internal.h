/*
 server_internal.h
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2026 Andy Alt

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

/* Types and prototypes shared between the server's .c files under src/server
 * but not part of the public server API in server.h. */

#ifndef __SERVER_INTERNAL_H
#define __SERVER_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include <deckhandler.h>

#include "types.h"

/* Outcome of a betting round: the winner id(s) the showdown awarded. */
typedef struct {
  uint8_t n_winners;
  int id[MAX_PLAYERS];
} RoundResults;

/* Loop-control result returned by the server's request handlers. */
typedef enum {
  LOOP_BREAK,
  LOOP_CONTINUE,
  LOOP_OK,
  LOOP_ERROR
} ELoop_t;

/* Outbound messaging (broadcast.c lives in server.c for now). */
void broadcast_game_state(ArgsBroadcastGameState_t *args);
void broadcast_turn_id(const ArgsBroadcastGameState_t *args);
void broadcast_status_message(const ArgsBroadcastGameState_t *args, const char *msg);
void broadcast_game_type(const ArgsBroadcastGameState_t *args);

uint8_t count_active_clients(const bool *slot_taken);

/* Betting engine + showdown (server.c), called by the game variants (games.c). */
void server_handle_ante(GameState_t *game_state, const uint32_t amount);
void handle_sort_hand(POKEVAL_Hand_9 *real_hand, const bool is_lowball, const bool deuces_wild);
ELoop_t handle_draw(ArgsBroadcastGameState_t *args, tcpme_socket_t sock, const int8_t id,
                    DH_Deck *deck);
void determine_winner(ArgsBroadcastGameState_t *args, RoundResults *results);
void award_last_player_in_game(ArgsBroadcastGameState_t *args, Player_t *turn, RoundResults *results);
RoundResults handle_round_real(ArgsBroadcastGameState_t *args, uint32_t initial_bet,
                               int8_t initial_paid_id);

/* Game flow (games.c), called by run_server/init_game (server.c). */
void play_game(ArgsBroadcastGameState_t *args, DH_Deck *deck);

#endif
