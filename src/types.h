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
#define SIZEOF_NICK 15
#define MAX_INPUT_LENGTH 64

#define LEN_STATUS_STR 100

typedef enum {
  RC_ERR = -1,
  RC_OK = 0,
} EReturnCode_t;

typedef enum {
  FIVE_CARD_DRAW,
  FIVE_CARD_DOUBLE_DRAW,
  FIVE_CARD_STUD,
  SEVEN_CARD_STUD,
  FIVE_CARD_SHOWDOWN,
  MAX_CHOICES,
} EMenuOption_t;

typedef struct {
  char nick[SIZEOF_NICK];
  int8_t id;
  POKEVAL_Hand_7 hand;
  int32_t coins;
  bool in; // Used for spectators or when someone has folded
  uint32_t total_paid;
  bool winner;
  bool has_checked;
  bool is_connected;
} Player_t;

typedef struct {
  uint32_t pot;
  int8_t dealer_id;
  uint8_t starting_turn_id;
  int8_t turn_id;
  bool at_menu;
  uint8_t player_count;
  uint32_t total_bets_plus_raises;
  bool winner_declared;
  bool deuces_wild;
  Player_t player[MAX_PLAYERS];
} GameState_t;

typedef struct {
  int8_t client_id;
  uint32_t action_timeout_ms;
  uint32_t end_of_game_timeout_ms;
} GameSettings_t;

typedef struct {
  POKEVAL_Hand_7 player[MAX_PLAYERS];
} RealHand_t;

typedef struct {
  const char *host, *server_conf, *bind_address;
  const char *server_log_game_results_file;
  bool test_mode, run_server_flag;
} CliArgs_t;

// A forward declaration
struct ServerConfig_t;

typedef struct {
  TCPsocket *clients;
  SDLNet_SocketSet socket_set;
  GameState_t *game_state;
  RealHand_t *real_hand;
  bool *slot_taken;
  CliArgs_t *cli_args;
  TCPsocket *server_sock;
  GameSettings_t *game_settings;
  struct ServerConfig_t *config;
  uint8_t game_type;
  Player_t **starting_turn;
} ArgsBroadcastGameState_t;

struct GameChoice_t;
typedef void (*game_func_t)(ArgsBroadcastGameState_t *, Player_t *, DH_Deck *,
                            const struct GameChoice_t *);

typedef struct GameChoice_t {
  const EMenuOption_t g;
  const char *str;
  const uint8_t game_type;
  game_func_t func;
  uint8_t hand_size;
  uint8_t n_betting_rounds, n_draws, n_stud_new_cards;
} GameChoice_t;

extern const GameChoice_t game_choices[];

#endif
