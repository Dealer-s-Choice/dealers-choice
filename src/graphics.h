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

#define SCALE_X(val) ((int)((val) * ui_scale.scale_x))
#define SCALE_Y(val) ((int)((val) * ui_scale.scale_y))

#define BUTTON_X_SPACING SCALE_X(10)

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
} EColorName_t;

SDL_Color get_color(EColorName_t name);
const char *get_color_name(EColorName_t name);

typedef struct {
  SDL_Renderer *renderer;
  SDL_Window *window;
  SDL_Point win_center;
  int window_width, window_height;
} SdlContext_t;
extern SdlContext_t *g_sdl_context;

typedef struct {
  float scale_x, scale_y;
} UiScale_t;
extern UiScale_t ui_scale;

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

void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                 SDL_Rect *dest);

typedef struct {
  const char *text;
  SDL_Renderer *renderer;
  SDL_Color bg_color;
  SDL_Color fg_color;
  SDL_Rect rect;
  TTF_Font *font;
  bool hovered, enabled, selected;
  bool active;
  SDL_Keycode hotkey;
} Button_t;

void mark_selected(SDL_Renderer *renderer, const SDL_Rect *rect);

void render_button(Button_t *button);

void render_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                          SDL_Point center);

void render_text_plain(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                       SDL_Rect *dest);

void render_nick(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                 SDL_Rect *dest, const bool is_turn);

SDL_Texture *load_texture(SDL_Renderer *renderer, const char *path);

bool toggle_fullscreen(SdlContext_t *sdl_context);

// Transitional loading screen
void show_loading_screen(SDL_Renderer *renderer, TTF_Font *font, const char *message);

#endif
