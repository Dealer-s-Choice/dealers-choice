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

#include "net.h"
#include "types.h"

typedef enum {
  COLOR_WHITE,
  COLOR_LIGHTGRAY,
  COLOR_GRAY,
  COLOR_DARKGRAY,
  COLOR_BLACK,
  COLOR_RED,
  COLOR_GREEN,
  COLOR_GREEN_ONE,
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
} ColorName;

SDL_Color get_color(ColorName name);
const char *get_color_name(ColorName name);

struct sdl_context_t {
  SDL_Renderer *renderer;
  SDL_Window *window;
  struct pos_t win_center;
  int window_width, window_height;
};

struct font_args_t {
  const char *file;
  const int ptsize;
};

enum { CARD, OTHER, NUM_FONTS };

extern const struct font_args_t font_args[NUM_FONTS];

struct font_t {
  TTF_Font *fonts[NUM_FONTS];
};

TTF_Font *open_font(const struct font_args_t *args);

void clear_screen(SDL_Renderer *renderer);

void init_sdl_window(struct sdl_context_t *sdl_context, const char *title, int w, int h);

SDL_Rect make_rect(int x, int y, int w, int h);

void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                 SDL_Rect *dest);

struct button_t {
  const char *text;
  SDL_Renderer *renderer;
  SDL_Color bg_color;
  SDL_Color fg_color;
  SDL_Rect rect;
  TTF_Font *font;
  bool hovered;
  bool enabled;
};

void render_button(struct button_t *button);

void render_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                          struct pos_t center);

void render_text_plain(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                       SDL_Rect *dest);

void do_sdl_cleanup(struct sdl_context_t *sdl_context);

#endif
