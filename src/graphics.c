/*
 graphics.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025,2026 Andy Alt

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

#include "graphics.h"
#include "ui_widget.h"
#include "widgets/text.h"

void show_loading_screen(SDL_Renderer *renderer, TTF_Font *font, const char *message) {
  (void)renderer;
  TextWidget_t *tw = text_widget_create(message, font, get_color(COLOR_WHITE));
  if (!tw)
    return;
  int cx = 640 - tw->base.rect.w / 2;
  int cy = 360 - tw->base.rect.h / 2;
  ui_widget_place(&tw->base, cx, cy);
  ui_widget_render(&tw->base);
  ui_widget_destroy(&tw->base);
}

static const SDL_Color color_table[COLOR_COUNT] = {
    [COLOR_WHITE] = {255, 255, 255, 255}, [COLOR_LIGHTGRAY] = {200, 200, 200, 255},
    [COLOR_GRAY] = {128, 128, 128, 255},  [COLOR_DARKGRAY] = {64, 64, 64, 255},
    [COLOR_BLACK] = {0, 0, 0, 255},       [COLOR_RED] = {255, 0, 0, 255},
    [COLOR_GREEN] = {0, 255, 0, 255},     [COLOR_TABLE_GREEN] = {0, 125, 0, 255},
    [COLOR_BLUE] = {0, 0, 255, 255},      [COLOR_YELLOW] = {255, 255, 0, 255},
    [COLOR_CYAN] = {0, 255, 255, 255},    [COLOR_MAGENTA] = {255, 0, 255, 255},
    [COLOR_ORANGE] = {255, 165, 0, 255},  [COLOR_PURPLE] = {128, 0, 128, 255},
    [COLOR_BROWN] = {165, 42, 42, 255},   [COLOR_PINK] = {255, 192, 203, 255},
    [COLOR_TEAL] = {0, 128, 128, 255},
};

static const char *color_names[COLOR_COUNT] = {
    "white",  "lightgray", "gray",    "darkgray", "black",  "red",   "green", "green_one", "blue",
    "yellow", "cyan",      "magenta", "orange",   "purple", "brown", "pink",  "teal"};

SDL_Color get_color(EColorName_t name) {
  if (name < 0 || name >= COLOR_COUNT)
    return (SDL_Color){0, 0, 0, 255}; // fallback
  return color_table[name];
}

const char *get_color_name(EColorName_t name) {
  if (name < 0 || name >= COLOR_COUNT)
    return "unknown";
  return color_names[name];
}

void clear_screen(SDL_Renderer *renderer) {
  SDL_SetRenderDrawColor(renderer, get_color(COLOR_TABLE_GREEN).r, get_color(COLOR_TABLE_GREEN).g,
                         get_color(COLOR_TABLE_GREEN).b, get_color(COLOR_TABLE_GREEN).a);
  SDL_RenderClear(renderer);
}

TTF_Font *open_font(const FontArgs_t *args) {
  TTF_Font *font = TTF_OpenFont(args->file, args->ptsize);
  if (font)
    return font;

  // fprintf(stderr, "Failed to load font (%s): %s\n", args->file, TTF_GetError());
  fprintf(stderr, "TTF_OpenFont: %s\n", SDL_GetError());
  exit(EXIT_FAILURE);
}

void mark_selected(SDL_Renderer *renderer, const SDL_Rect *rect) {
  const float outer_thickness = 4.0f;
  const float inner_thickness = 2.0f;

  SDL_FRect frect = {
      .x = (float)rect->x,
      .y = (float)rect->y,
      .w = (float)rect->w,
      .h = (float)rect->h,
  };

  // Outer border (light grey, expanding outward)
  SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
  for (float i = 0; i < outer_thickness; i += 1.0f) {
    SDL_FRect border = {frect.x - i, frect.y - i, frect.w + 2.0f * i, frect.h + 2.0f * i};
    SDL_RenderDrawRectF(renderer, &border);
  }

  // Inner border (black, shrinking inward)
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  for (float i = 0; i < inner_thickness; i += 1.0f) {
    SDL_FRect border = {frect.x + i, frect.y + i, frect.w - 2.0f * i, frect.h - 2.0f * i};
    if (border.w > 0.0f && border.h > 0.0f) {
      SDL_RenderDrawRectF(renderer, &border);
    }
  }
}

SDL_Texture *load_texture(SDL_Renderer *renderer, const char *path) {
  if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
    SDL_Log("IMG_Init failed: %s", IMG_GetError());
    exit(EXIT_FAILURE);
  }

  SDL_Surface *surface = IMG_Load(path);
  if (!surface) {
    SDL_Log("IMG_Load failed: %s", IMG_GetError());
    exit(EXIT_FAILURE);
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    SDL_Log("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  SDL_FreeSurface(surface);
  return texture;
}

bool toggle_fullscreen(SdlContext_t *c) {
  if (!c || !c->window) {
    SDL_Log("toggle_fullscreen: invalid context");
    return false;
  }
  const Uint32 flags = SDL_GetWindowFlags(c->window);
  const bool is_fs = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
  const Uint32 new_mode = is_fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
  if (SDL_SetWindowFullscreen(c->window, new_mode) != 0) {
    SDL_Log("SDL_SetWindowFullscreen failed: %s", SDL_GetError());
    return false;
  }
  SDL_RenderGetViewport(c->renderer, &g_viewport);
  g_center.x = g_viewport.x + g_viewport.w / 2;
  g_center.y = g_viewport.y + g_viewport.h / 2;
  return true;
}

// Removes a little from the border, this function is primarily intended
// for use with input boxes, the prevent the borders from disappearing.
void draw_rect_border(SDL_Renderer *r, SDL_Rect rect) {
  rect.w--;
  rect.h--;
  SDL_RenderDrawRect(r, &rect);
}
