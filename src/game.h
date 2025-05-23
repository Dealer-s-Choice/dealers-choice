/*
 game.h
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

#ifndef __GAME_H
#define __GAME_H

// #include <SDL2/SDL_mixer.h>
#include <pokeval.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "graphics.h"
#include "net.h"

#define MSG_GAME_SELECT 0x0001       // struct player_t chooses a game variant
#define MSG_PLAYER_ACTION 0x0002     // struct player_t bets, folds, etc.
#define MSG_GAME_START 0x0004        // Game begins
#define MSG_DEAL_CARDS 0x0005        // Cards sent to player
#define MSG_DRAW_REQUEST 0x0006      // struct player_t discards cards for draw
#define MSG_GAME_STATE_UPDATE 0x0007 // Server sends state update

typedef enum {
  ACTION_INVALID = 0x00,
  ACTION_CHECK = 0x01,
  ACTION_CALL = 0x02,
  ACTION_BET = 0x03,
  ACTION_RAISE = 0x04,
  ACTION_FOLD = 0x05
} player_action_t;

typedef enum {
  GAME_INVALID = 0x00,
  GAME_5_CARD_DRAW = 0x01,
  GAME_5_CARD_DOUBLE = 0x02,
  GAME_5_CARD_STUD = 0x03,
  GAME_7_CARD_STUD = 0x04
} game_type_t;

#define MSG_PLAYER_ACTION 0x0002
#define SIZE_MESSAGE_GAME_SELECT 3

struct player_action_msg_t {
  uint8_t action;
  uint32_t amount; // only used for bet/raise
};

struct player_t *get_next_player(struct player_t (*players_array)[MAX_PLAYERS], const int cur);

void run_sdl_loop(Game_State *game_state, struct sdl_context_t *sdl_context, struct font_t *font,
                  TCPsocket client_socket, SDLNet_SocketSet socket_set, const uint8_t my_id);

typedef struct {
  char str[200];
} DebugPrintCards_t;

DebugPrintCards_t debug_print_cards(struct pokeval_hand_t *hand);

#endif
