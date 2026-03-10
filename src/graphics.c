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
#include "game.h"

void show_loading_screen(SDL_Renderer *renderer, TTF_Font *font, const char *message) {
  SDL_Color color = get_color(COLOR_WHITE);
  SDL_Rect rect = {0, 0, 0, 0};
  int w = 0, h = 0;
  if (TTF_SizeUTF8(font, message, &w, &h) == 0) {
    rect.x = 640 - w / 2;
    rect.y = 360 - h / 2;
    rect.w = w;
    rect.h = h;
  } else {
    rect.x = 640;
    rect.y = 360;
    rect.w = 0;
    rect.h = 0;
  }
  render_text_plain(renderer, font, message, color, &rect);
}

static const SDL_Color color_table[COLOR_COUNT] = {
    [COLOR_WHITE] = {255, 255, 255, 255}, [COLOR_LIGHTGRAY] = {200, 200, 200, 255},
    [COLOR_GRAY] = {128, 128, 128, 255},  [COLOR_DARKGRAY] = {64, 64, 64, 255},
    [COLOR_BLACK] = {0, 0, 0, 255},       [COLOR_RED] = {255, 0, 0, 255},
    [COLOR_GREEN] = {0, 255, 0, 255},     [COLOR_GREEN_ONE] = {0, 125, 0, 255},
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
  SDL_SetRenderDrawColor(renderer, get_color(COLOR_GREEN_ONE).r, get_color(COLOR_GREEN_ONE).g,
                         get_color(COLOR_GREEN_ONE).b, get_color(COLOR_GREEN_ONE).a);
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

void render_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                          SDL_Point center) {
  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
  if (!surface) {
    fprintf(stderr, "TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  int w, h;
  SDL_QueryTexture(texture, NULL, NULL, &w, &h);

  SDL_Rect dst = {.x = center.x - w / 2, .y = center.y - h / 2, .w = w, .h = h};

  SDL_RenderCopy(renderer, texture, NULL, &dst);

  SDL_DestroyTexture(texture);
  SDL_FreeSurface(surface);
}

void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                 SDL_Rect *dest) {
  if (!text)
    text = "";

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, *text ? text : " ", color);
  if (!surface) {
    fprintf(stderr, "TTF_RenderUTF8_Blended error: %s\n", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  dest->w = surface->w;
  dest->h = surface->h;

  SDL_RenderCopy(renderer, texture, NULL, dest);

  int text_width;
  TTF_SizeUTF8(font, text, &text_width, NULL);

  // Blink cursor every ~500ms
  if ((SDL_GetTicks() / 500) % 2 == 0) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    int cursor_x = dest->x + text_width;
    int cursor_y = dest->y;
    int cursor_h = dest->h;
    SDL_RenderDrawLine(renderer, cursor_x, cursor_y, cursor_x, cursor_y + cursor_h);
  }

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

void render_text_plain(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                       SDL_Rect *dest) {
  if (!text)
    text = "";

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, *text ? text : " ", color);
  if (!surface) {
    fprintf(stderr, "TTF_RenderUTF8_Blended error: %s\n", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  dest->w = surface->w;
  dest->h = surface->h;

  SDL_RenderCopy(renderer, texture, NULL, dest);

  int text_width;
  if (TTF_SizeUTF8(font, text, &text_width, NULL) != 0)
    fprintf(stderr, "TTF_SizeUTF8 failed: %s\n", TTF_GetError());

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

void render_nick(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                 SDL_Rect *dest, const bool is_turn) {
  if (!text)
    text = "";

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, *text ? text : " ", color);
  if (!surface) {
    fprintf(stderr, "TTF_RenderUTF8_Blended error: %s\n", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  dest->w = surface->w;
  dest->h = surface->h;

  // Optional blinking background if it's their turn
  if (is_turn && (SDL_GetTicks() / 500) % 2 == 0) { // Blinks every 500ms
    SDL_Color blink_color = {255, 255, 0, 64};      // Yellow, transparent
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, blink_color.r, blink_color.g, blink_color.b, blink_color.a);
    SDL_RenderFillRect(renderer, dest);
  }

  SDL_RenderCopy(renderer, texture, NULL, dest);

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
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

static void draw_filled_ellipse(SDL_Renderer *r, int cx, int cy, int rx, int ry) {
  if (rx <= 0 || ry <= 0)
    return;

  for (int y = -ry; y <= ry; y++) {
    float dy = (float)y / (float)ry;

    float inside = 1.0f - dy * dy;
    if (inside < 0.0f)
      continue;

    float dx = rx * sqrtf(inside);
    int x = (int)(dx + 0.5f); // round safely

    SDL_RenderDrawLine(r, cx - x, cy + y, cx + x, cy + y);
  }
}

void render_indicator(const Indicator_t *ind) {
  if (!ind || !ind->text)
    return;

  SDL_Renderer *r = ind->renderer;

  // Center and radii
  int cx = ind->rect.x + ind->rect.w / 2;
  int cy = ind->rect.y + ind->rect.h / 2;
  int rx = ind->rect.w / 2;
  int ry = ind->rect.h / 2;

  // Draw oval background
  SDL_SetRenderDrawColor(r, ind->bg_color.r, ind->bg_color.g, ind->bg_color.b, ind->bg_color.a);
  draw_filled_ellipse(r, cx, cy, rx, ry);

  // Render text
  SDL_Surface *surf = TTF_RenderUTF8_Blended(ind->font, ind->text, ind->fg_color);
  if (!surf)
    return;

  SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
  if (!tex) {
    SDL_FreeSurface(surf);
    return;
  }

  SDL_Rect text_rect = {cx - surf->w / 2, cy - surf->h / 2, surf->w, surf->h};

  SDL_RenderCopy(r, tex, NULL, &text_rect);

  SDL_FreeSurface(surf);
  SDL_DestroyTexture(tex);
}

Indicator_t create_indicator(SDL_Renderer *renderer, const char *text, const Font_t *font) {
  Indicator_t ind = {
      .text = text,
      .renderer = renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_ORANGE),
      .rect = {0},
      .font = font->fonts[FONT_BOLD],
  };

  if (TTF_SizeUTF8(ind.font, ind.text, &ind.rect.w, &ind.rect.h) != 0) {
    fprintf(stderr, "TTF_SizeUTF8 failed: %s\n", TTF_GetError());
    ind.rect.w = 40;
    ind.rect.h = 20;
  }

  int PAD_X = ind.rect.h;     // one text-height on each side
  int PAD_Y = ind.rect.h / 3; // subtle vertical padding

  ind.rect.w += PAD_X * 2;
  ind.rect.h += PAD_Y * 2;

  return ind;
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

  return true;
}

// Removes a little from the border, this function is primarily intended
// for use with input boxes, the prevent the borders from disappearing.
void draw_rect_border(SDL_Renderer *r, SDL_Rect rect) {
  rect.w--;
  rect.h--;
  SDL_RenderDrawRect(r, &rect);
}
