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

#include "config.h"
#include "globals.h"
#include "graphics.h"
#include "net.h"
#include "server.h"
#include "translate.h"
#include "types.h"

#define get_next_player(a, b) get_next_player_(a, b, __FILE__, __LINE__)
#define get_next_connected_client(a, b) get_next_connected_client_(a, b, __FILE__, __LINE__)

typedef enum {
  ACTION_INVALID = 0x00,
  ACTION_CHECK = 0x01,
  ACTION_CALL = 0x02,
  ACTION_BET = 0x03,
  ACTION_RAISE = 0x04,
  ACTION_FOLD = 0x05
} EPlayerAction_t;

#define MSG_PLAYER_ACTION 0x0002
#define SIZE_MESSAGE_GAME_SELECT 4

typedef struct {
  uint8_t action;
  uint32_t amount; // only used for bet/raise
  const char *str;
} PlayerActionMsg_t;

typedef struct {
  uint8_t game_type;
  uint8_t deuces_wild;
} GameSelectPayload_t;

const GameChoice_t *find_game_choice_by_type(const uint8_t type);

int8_t send_game_select(TCPsocket sock, uint8_t game_type, bool deuces_wild);

bool get_game_select_payload(uint8_t *buffer, const uint32_t size, const int client_id,
                             GameSelectPayload_t *out);

Player_t *get_next_player_(Player_t *players_array, int cur, const char *file, const int line);

Player_t *get_next_connected_client_(Player_t *players_array, int cur, const char *file,
                                     const int line);

void pcg_srand_auto(void);

#endif
