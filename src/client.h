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
#include <miniaudio.h>

#include "dc_config.h"
#include "graphics.h"
#include "net.h"
#include "types.h"

typedef struct {
  const char *text;
  const char *url;
  TTF_Font *font;
  // SDL_Color textColor;
  SDL_Renderer *renderer;
  // SDL_Color bg_color;
  // SDL_Color fg_color;
  SDL_Rect rect;
  bool hovered;
} Link_t;

typedef enum {
  SND_SERVER_JOIN,
  SND_CARD_DEALT,
  SND_MY_TURN,
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
  size_t coin_array_size;
} SoundContext_t;

extern const GameChoice_t game_choices[];

SocketContext_t get_socket_context_and_run_client(PlayerConfig_t *player_config,
                                                  const char *host_str, SdlContext_t *sdl_context,
                                                  Font_t *font, Path_t *path, const bool test_mode);

void render_link(Link_t *link);

int8_t send_game_select(TCPsocket sock, uint8_t game_type, const bool deuces_wild);

int8_t send_player_action(TCPsocket sock, uint8_t action, uint32_t amount);

void do_sdl_cleanup(SdlContext_t *sdl_context);

int8_t send_discards_request_new_cards(TCPsocket sock, const uint8_t *discard_indices,
                                              uint8_t count);
