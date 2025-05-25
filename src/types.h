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

typedef enum {
  GAME_INVALID = 0x00,
  GAME_5_CARD_DRAW = 0x01,
  GAME_5_CARD_DOUBLE = 0x02,
  GAME_5_CARD_STUD = 0x03,
  GAME_7_CARD_STUD = 0x04
} game_type_t;

typedef enum {
  FIVE_CARD_DRAW,
  FIVE_CARD_STUD,
  MAX_CHOICES,
} menu_option_t;

struct pos_t {
  int x;
  int y;
};

struct player_t {
  char name[256];
  int8_t id;
  struct pokeval_hand_t hand;
  int32_t coins;
  bool in; // Used for spectators or when someone has folded
  uint32_t total_paid;
  bool winner;
  bool has_checked;
};

typedef struct {
  uint32_t pot;
  int8_t dealer_id;
  int32_t turn_id;
  bool at_menu;
  uint8_t player_count;
  uint32_t total_bets_plus_raises;
  bool round_over;
  bool winner_declared;
  uint8_t n_rounds;
  char status_str[512];
  struct player_t player[MAX_PLAYERS];
} Game_State;

typedef struct {
  struct pokeval_hand_t player[MAX_PLAYERS];
} RealHand;

typedef struct {
  TCPsocket (*clients)[MAX_CLIENTS];
  SDLNet_SocketSet *socket_set;
  int *active_clients;
  Game_State *game_state;
  RealHand *real_hand;
  bool (*slot_taken)[MAX_CLIENTS];
} args_broadcast_game_state_t;

typedef void (*game_func_t)(args_broadcast_game_state_t *, struct player_t *, struct player_t *, struct dh_deck *);

typedef struct {
  const menu_option_t g;
  const char *str;
  const game_type_t game_type;
  game_func_t func;
} GameChoice;

extern const GameChoice game_choices[];

#endif
