/*
 types.h
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025 Andy Alt

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

#ifndef __TYPES_H
#define __TYPES_H

#include <pokeval.h>

#define MAX_PLAYERS 5
#define MAX_CLIENTS 5

#define LEN_STATUS_STR 100

typedef enum {
  FIVE_CARD_DRAW,
  FIVE_CARD_DOUBLE_DRAW,
  FIVE_CARD_STUD,
  FIVE_CARD_SHOWDOWN,
  MAX_CHOICES,
} EMenuOption_t;

typedef struct {
  char name[12];
  int8_t id;
  struct pokeval_hand_t hand;
  int32_t coins;
  bool in; // Used for spectators or when someone has folded
  uint32_t total_paid;
  bool winner;
  bool has_checked;
} Player_t;

typedef struct {
  uint32_t pot;
  int8_t dealer_id;
  int8_t turn_id;
  bool at_menu;
  uint8_t player_count;
  uint32_t total_bets_plus_raises;
  bool winner_declared;
  int32_t action_time_out_ms;
  int32_t end_of_round_time_out_ms;
  Player_t player[MAX_PLAYERS];
} GameState_t;

typedef struct {
  struct pokeval_hand_t player[MAX_PLAYERS];
} RealHand_t;

typedef struct {
  TCPsocket (*clients)[MAX_CLIENTS];
  SDLNet_SocketSet *socket_set;
  int *active_clients;
  GameState_t *game_state;
  RealHand_t *real_hand;
  bool (*slot_taken)[MAX_CLIENTS];
} ArgsBroadcastGameState_t;

typedef void (*game_func_t)(ArgsBroadcastGameState_t *, Player_t *, Player_t *, DH_Deck *, uint8_t,
                            uint8_t);

typedef struct {
  const EMenuOption_t g;
  const char *str;
  const uint8_t game_type;
  game_func_t func;
  uint8_t n_betting_rounds, draws;
} GameChoice_t;

extern const GameChoice_t game_choices[];

#endif
