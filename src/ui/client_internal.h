/*
 client_internal.h
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

/* Types and prototypes shared between client.c (connection/setup/teardown) and
 * game_logic.c (the gameplay-screen render+input loop), but not part of the
 * public client API in client.h. */

#ifndef __CLIENT_INTERNAL_H
#define __CLIENT_INTERNAL_H

#include "client.h"

/* Start a miniaudio sound, logging the call site on failure.  ma_sound_start_wrap
 * lives in client.c; both client.c and game_logic.c trigger sounds. */
void ma_sound_start_wrap(ma_sound *pSound, const char *file, const int line);
#define ma_sound_start_checked(pSound) ma_sound_start_wrap((pSound), __FILE__, __LINE__)

/* Diff a fresh game state against the previous connected-set, marking which
 * slots just joined/left.  Lives in client.c; used by both the lobby and the
 * gameplay loop. */
void detect_player_changes(const GameState_t *gs, bool was_connected[MAX_PLAYERS],
                           bool joined[MAX_PLAYERS], bool left[MAX_PLAYERS]);

/* Result of one gameplay-screen pass (game_logic.c), inspected by the caller
 * in client.c's connection loop. */
typedef enum {
  GAME_LOGIC_QUIT = 0,
  GAME_LOGIC_AT_MENU = 1,
  GAME_LOGIC_DISCONNECTED = 2,
} EGameLogicResult_t;

/* Result of the lobby / game-selection screen (game_select.c). */
typedef enum {
  GAME_SEL_ERROR = 0,
  GAME_SEL_SUCCESS = 1,
  GAME_SEL_BACK = 2,
  GAME_SEL_QUIT = 3,
} EGameSelResult_t;

/* The pre-game lobby / game-selection screen (game_select.c), driven by
 * client.c's connection loop. */
EGameSelResult_t handle_game_selection(const PlayerConfig_t *player_config,
                                       SocketContext_t *socket_context, const int8_t my_id,
                                       GameState_t *game_state, ClientState_t *client_state,
                                       SdlContext_t *sdl_context, Font_t *font,
                                       const SoundContext_t *sound_context, LinkWidget_t **links,
                                       const Path_t *path);

/* Lazy coin/texture loader; lives in client.c (setup-time resource loading) but
 * is also called by the gameplay loop to fetch the felt-tile texture. */
SDL_Texture *load_coin_texture(SDL_Renderer *renderer, const char *base_path, const char *coin);

/* The gameplay-screen render+input loop (game_logic.c), driven by client.c's
 * connection loop. */
EGameLogicResult_t handle_game_logic(const PlayerConfig_t *player_config,
                                     SocketContext_t *socket_context,
                                     const GameSettings_t *game_settings, GameState_t *game_state,
                                     SdlContext_t *sdl_context, const Font_t *font,
                                     const SoundContext_t *sound_context,
                                     SDL_Texture **coin_textures, size_t n_coin_images);

#endif
