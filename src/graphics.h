/*
 graphics.h
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

#ifndef __GRAPHICS_H
#define __GRAPHICS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL_image.h>

#include "net.h"
#include "types.h"

#define LOGICAL_WIDTH 1920
#define LOGICAL_HEIGHT 1080

typedef enum {
  COLOR_WHITE,
  COLOR_LIGHTGRAY,
  COLOR_GRAY,
  COLOR_DARKGRAY,
  COLOR_BLACK,
  COLOR_RED,
  COLOR_GREEN,
  COLOR_TABLE_GREEN,
  COLOR_BLUE,
  COLOR_YELLOW,
  COLOR_CYAN,
  COLOR_MAGENTA,
  COLOR_ORANGE,
  COLOR_PURPLE,
  COLOR_BROWN,
  COLOR_PINK,
  COLOR_TEAL,
  COLOR_COUNT // keep this last
} EColorName_t;

SDL_Color get_color(EColorName_t name);
const char *get_color_name(EColorName_t name);

typedef struct {
  SDL_Renderer *renderer;
  SDL_Window *window;
} SdlContext_t;
extern SdlContext_t *g_sdl_context;

typedef struct {
  const char *file;
  const int ptsize;
} FontArgs_t;

enum {
  FONT_CARD,
  FONT_DEFAULT,
  FONT_DEFAULT_BOLD,
  FONT_BOLD,
  FONT_LINK,
  FONT_STATUS_MSG,
  FONT_TITLE,
  FONT_VERSION,
  FONT_WILD_SELECT,
  NUM_FONTS
};

// extern const FontArgs_t font_args[NUM_FONTS];

typedef struct {
  TTF_Font *fonts[NUM_FONTS];
} Font_t;

TTF_Font *open_font(const FontArgs_t *args);

void clear_screen(SDL_Renderer *renderer);
void draw_felt_background(SDL_Renderer *renderer, SDL_Texture *felt_tile);
bool confirm_quit(TTF_Font *const *fonts);
void draw_nameplate(SDL_Renderer *r, SDL_Rect rect, uint8_t alpha);
SDL_Texture *create_vignette_texture(SDL_Renderer *renderer);

void mark_selected(SDL_Renderer *renderer, const SDL_Rect *rect);

SDL_Texture *load_texture(SDL_Renderer *renderer, const char *path);

bool toggle_fullscreen(SdlContext_t *sdl_context);

// Transitional loading screen
void show_loading_screen(SDL_Renderer *renderer, TTF_Font *font, const char *message);

void draw_rect_border(SDL_Renderer *r, SDL_Rect rect);
void draw_3d_border(SDL_Renderer *r, SDL_Rect rect, int thickness);

#endif
