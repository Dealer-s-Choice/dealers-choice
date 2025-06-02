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
#include "graphics.h"
#include "net.h"
#include "server.h"
#include "types.h"

typedef enum {
  ACTION_INVALID = 0x00,
  ACTION_CHECK = 0x01,
  ACTION_CALL = 0x02,
  ACTION_BET = 0x03,
  ACTION_RAISE = 0x04,
  ACTION_FOLD = 0x05
} player_action_t;

#define MSG_PLAYER_ACTION 0x0002
#define SIZE_MESSAGE_GAME_SELECT 3

struct player_action_msg_t {
  uint8_t action;
  uint32_t amount; // only used for bet/raise
};

const GameChoice_t *find_game_choice_by_type(const uint8_t type);

Player_t *get_next_player(Player_t *players_array, int cur);

bool is_dh_card_back(DH_Card a);

bool is_dh_card_null(DH_Card a);

void render_project_link(SDL_Renderer *renderer, TTF_Font *font, SDL_Rect *rect,
                         const bool hovered);

void run_sdl_loop(ClientState_t *client_state, SdlContext_t *sdl_context, Font_t *font,
                  TCPsocket client_socket, SDLNet_SocketSet socket_set, const uint8_t my_id);

int8_t send_game_select(TCPsocket sock, uint8_t game_type);

int8_t send_player_action(TCPsocket sock, uint8_t action, uint32_t amount);

extern const GameChoice_t game_choices[];

#endif
