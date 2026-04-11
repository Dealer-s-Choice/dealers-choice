/*
 client.h
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

#include <deckhandler.h>
#include <miniaudio/miniaudio.h>

#include "dc_config.h"
#include "graphics.h"
#include "links.h"
#include "net.h"
#include "types.h"
#include "widgets/card.h"

typedef enum {
  SND_SERVER_JOIN,
  SND_MY_TURN,
  SND_GAME_OVER,
  SND_NUM_SOUNDS,
} ESndIdx_t;

typedef struct {
  const char *filename;
  ma_sound sound;
} Sound_t;

typedef struct {
  ma_result result;
  ma_engine engine;
  ma_engine_config engineConfig;
  Sound_t *sounds;
  Sound_t *coin_hit_sounds;
  uint32_t coin_array_size;
} SoundContext_t;

extern const GameChoice_t game_choices[];

bool get_socket_context_and_run_client(PlayerConfig_t *player_config, const CliArgs_t *cli_args,
                                       const char *host_str, const uint16_t port,
                                       SdlContext_t *sdl_context, Font_t *font, Path_t *path,
                                       const bool test_mode, LinkWidget_t **links,
                                       SocketContext_t *out_socket_context);

int send_player_action(ClientState_t *client_state, TCPsocket sock, uint8_t action,
                       uint32_t amount);

void do_sdl_cleanup(SdlContext_t *sdl_context);

int send_discards_request_new_cards(TCPsocket sock, const uint8_t *discard_indices, uint8_t count);

void layout_cards(CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE], Player_t *players_array,
                  const SDL_Point *player_pos);

int authenticate_with_server(TCPsocket sock, const char *password);

bool bot_connect(const char *host_str, uint16_t port, const char *nick, const char *password,
                 SocketContext_t *out);
