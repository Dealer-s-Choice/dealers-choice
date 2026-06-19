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
typedef enum { LOOP_BREAK, LOOP_CONTINUE, LOOP_OK, LOOP_ERROR } ELoop_t;

/* Classification of a message read from the turn player's socket. */
typedef enum {
  TURN_MSG_ACTION,   /* MSG_PLAYER_ACTION — game action from the turn player */
  TURN_MSG_KICK_BAN, /* MSG_KICK_PLAYER / MSG_BAN_PLAYER from an admin who happens to be on turn */
  TURN_MSG_DISCONNECT, /* connection closed or unrecognised data */
} ETurnMsg_t;

/* Outbound messaging (broadcast.c lives in server.c for now). */
void broadcast_game_state(ArgsBroadcastGameState_t *args);
void broadcast_turn_id(const ArgsBroadcastGameState_t *args);
void broadcast_status_message(const ArgsBroadcastGameState_t *args, const char *msg);
void broadcast_action_announce(const ArgsBroadcastGameState_t *args, int8_t player_id, int verb,
                              uint32_t amount);
void broadcast_game_type(const ArgsBroadcastGameState_t *args);

uint8_t count_active_clients(const bool *slot_taken);

/* Betting engine + showdown (server.c), called by the game variants (variants.c). */
void server_handle_ante(GameState_t *game_state, const uint32_t amount);
void handle_sort_hand(POKEVAL_Hand_9 *real_hand, const bool is_lowball, const bool deuces_wild);
ELoop_t handle_draw(ArgsBroadcastGameState_t *args, tcpme_socket_t sock, const int8_t id,
                    DH_Deck *deck);
void determine_winner(ArgsBroadcastGameState_t *args, RoundResults *results);
void award_last_player_in_game(ArgsBroadcastGameState_t *args, Player_t *turn,
                               RoundResults *results);
RoundResults handle_round_real(ArgsBroadcastGameState_t *args, uint32_t initial_bet,
                               int8_t initial_paid_id);

/* Game flow (variants.c), called by run_server/init_game (server.c). */
void play_game(ArgsBroadcastGameState_t *args, DH_Deck *deck);

/* Messaging and client-management helpers that live in server.c but are also
 * called from the betting/showdown engine (round.c). */
int send_opcode(tcpme_socket_t sock, const uint16_t opcode);
int send_new_hand(tcpme_socket_t sock, const POKEVAL_Hand_9 *hand, uint8_t hand_size);
ETurnMsg_t recv_turn_player_msg(tcpme_socket_t sock, PlayerActionMsg_t *out_action,
                                uint16_t *out_kb_opcode, int8_t *out_target_id);
void maybe_log_day_header(const char *path);
void log_hands_json(const ArgsBroadcastGameState_t *args, const POKEVAL_NeedComparing *cmp,
                    uint8_t pl_count, uint32_t pot, bool by_fold);
void log_hands_fold_json(const ArgsBroadcastGameState_t *args, const Player_t *winner,
                         uint32_t pot);
void remove_disconnected_player(ArgsBroadcastGameState_t *args, const int8_t id);
void kick_player(ArgsBroadcastGameState_t *args, int8_t id);
void ban_player(ArgsBroadcastGameState_t *args, int8_t id);
bool handle_disconnections(ArgsBroadcastGameState_t *args);
ELoop_t register_new_client(ArgsBroadcastGameState_t *args);

#endif
