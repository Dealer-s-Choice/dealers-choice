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

#include <math.h>
#include <stdlib.h>

#include "graphics.h"
#include "util.h"
#include "layout.h"
#include "style.h"
#include "translate.h"
#include "ui_widget.h"
#include "widgets/button.h"
#include "widgets/text.h"

void show_loading_screen(SDL_Renderer *renderer, TTF_Font *font, const char *message) {
  (void)renderer;
  TextWidget_t *tw = text_widget_create(message, font, DC_TEXT_ON_DARK);
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
    [COLOR_TEAL] = {0, 128, 128, 255},   [COLOR_GOLD] = {233, 196, 106, 255},
};

static const char *color_names[COLOR_COUNT] = {
    "white",  "lightgray", "gray",    "darkgray", "black",  "red",   "green", "green_one", "blue",
    "yellow", "cyan",      "magenta", "orange",   "purple", "brown", "pink",  "teal", "gold"};

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

bool confirm_quit(TTF_Font *const *fonts) {
  if (!g_sdl_context || !fonts)
    return false;

  SDL_Renderer *r = g_sdl_context->renderer;

  TextWidget_t *msg_tw =
      text_widget_create(_("Are you sure you want to quit?"), fonts[FONT_BOLD], DC_TEXT_ON_DARK);
  ButtonWidget_t *btn_cancel =
      button_widget_create_styled(_("Cancel"), &ROLE_CANCEL, fonts, (SDL_Keycode)0);
  ButtonWidget_t *btn_quit_w =
      button_widget_create_styled(_("Quit"), &ROLE_DANGER, fonts, (SDL_Keycode)0);

  if (!msg_tw || !btn_cancel || !btn_quit_w) {
    if (msg_tw)
      ui_widget_destroy(&msg_tw->base);
    if (btn_cancel)
      ui_widget_destroy(&btn_cancel->base);
    if (btn_quit_w)
      ui_widget_destroy(&btn_quit_w->base);
    return false;
  }

  const int pad = g_layout_cfg.confirm_quit_pad;
  const int btn_gap = g_layout_cfg.confirm_quit_btn_gap;
  const int dialog_w =
      SDL_max(msg_tw->base.rect.w + pad * 2,
              btn_cancel->base.rect.w + btn_gap + btn_quit_w->base.rect.w + pad * 2);
  const int dialog_h = pad + msg_tw->base.rect.h + pad + btn_cancel->base.rect.h + pad;
  SDL_Rect dialog = {g_center.x - dialog_w / 2, g_center.y - dialog_h / 2, dialog_w, dialog_h};

  ui_widget_place(&msg_tw->base, g_center.x - msg_tw->base.rect.w / 2, dialog.y + pad);

  const int btns_total_w = btn_cancel->base.rect.w + btn_gap + btn_quit_w->base.rect.w;
  const int btn_y = dialog.y + dialog_h - btn_cancel->base.rect.h - pad;
  ui_widget_place(&btn_cancel->base, g_center.x - btns_total_w / 2, btn_y);
  ui_widget_place(&btn_quit_w->base,
                  g_center.x - btns_total_w / 2 + btn_cancel->base.rect.w + btn_gap, btn_y);

  bool result = false;
  bool running = true;

  while (running) {
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    float lx, ly;
    SDL_RenderWindowToLogical(r, mx, my, &lx, &ly);
    SDL_Point mouse_pos = {(int)lx, (int)ly};
    btn_cancel->base.hovered = SDL_PointInRect(&mouse_pos, &btn_cancel->base.rect);
    btn_quit_w->base.hovered = SDL_PointInRect(&mouse_pos, &btn_quit_w->base.rect);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
    SDL_RenderFillRect(r, &dialog);
    draw_3d_border(r, dialog, 4);

    ui_widget_render(&msg_tw->base);
    ui_widget_render(&btn_cancel->base);
    ui_widget_render(&btn_quit_w->base);
    SDL_RenderPresent(r);
    SDL_Delay(16);

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        SDL_PushEvent(&e);
        result = true;
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (SDL_PointInRect(&mouse_pos, &btn_quit_w->base.rect)) {
          result = true;
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &btn_cancel->base.rect)) {
          running = false;
        }
      } else if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
        case SDLK_ESCAPE:
          running = false;
          break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
          result = true;
          running = false;
          break;
        }
      }
    }
  }

  ui_widget_destroy(&msg_tw->base);
  ui_widget_destroy(&btn_cancel->base);
  ui_widget_destroy(&btn_quit_w->base);
  return result;
}

void draw_nameplate(SDL_Renderer *r, SDL_Rect rect, uint8_t alpha) {
  const int radius = g_layout_cfg.nameplate_radius;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 0, 0, 0, alpha);

  // Center horizontal strip + left/right side strips
  SDL_RenderFillRect(r, &(SDL_Rect){rect.x + radius, rect.y, rect.w - 2 * radius, rect.h});
  SDL_RenderFillRect(r, &(SDL_Rect){rect.x, rect.y + radius, radius, rect.h - 2 * radius});
  SDL_RenderFillRect(
      r, &(SDL_Rect){rect.x + rect.w - radius, rect.y + radius, radius, rect.h - 2 * radius});

  // Rounded corners via scanlines
  for (int dy = 0; dy < radius; dy++) {
    int dx = (int)sqrtf((float)(radius * radius - dy * dy) + 0.5f);
    int top_y = rect.y + radius - 1 - dy;
    int bot_y = rect.y + rect.h - radius + dy;
    SDL_RenderFillRect(r, &(SDL_Rect){rect.x + radius - dx, top_y, dx, 1});
    SDL_RenderFillRect(r, &(SDL_Rect){rect.x + rect.w - radius, top_y, dx, 1});
    SDL_RenderFillRect(r, &(SDL_Rect){rect.x + radius - dx, bot_y, dx, 1});
    SDL_RenderFillRect(r, &(SDL_Rect){rect.x + rect.w - radius, bot_y, dx, 1});
  }

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

SDL_Texture *create_vignette_texture(SDL_Renderer *renderer) {
  const int w = LOGICAL_WIDTH;
  const int h = LOGICAL_HEIGHT;

  SDL_Texture *tex =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (!tex)
    return NULL;
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

  Uint32 *pixels = malloc((size_t)w * h * sizeof(Uint32));
  if (!pixels) {
    SDL_DestroyTexture(tex);
    return NULL;
  }

  const float cx = w / 2.0f;
  const float cy = h / 2.0f;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float dx = (x - cx) / cx;
      float dy = (y - cy) / cy;
      float d = sqrtf(dx * dx + dy * dy) / sqrtf(2.0f); // 0 at center, 1 at corner
      float a = d * d * 220.0f;
      if (a > 220.0f)
        a = 220.0f;
      pixels[y * w + x] = (Uint32)((Uint8)a) << 24; // ARGB: black, varying alpha
    }
  }

  SDL_UpdateTexture(tex, NULL, pixels, w * (int)sizeof(Uint32));
  free(pixels);
  return tex;
}

void draw_felt_background(SDL_Renderer *renderer, SDL_Texture *felt_tile) {
  const int tile_w = 100;
  const int tile_h = 100;
  for (int y = 0; y < LOGICAL_HEIGHT; y += tile_h) {
    for (int x = 0; x < LOGICAL_WIDTH; x += tile_w) {
      SDL_Rect dst = {x, y, tile_w, tile_h};
      SDL_RenderCopy(renderer, felt_tile, NULL, &dst);
    }
  }
}

TTF_Font *open_font(const FontArgs_t *args) {
  TTF_Font *font = TTF_OpenFont(args->file, args->ptsize);
  if (font)
    return font;

  // fprintf(stderr, "Failed to load font (%s): %s\n", args->file, TTF_GetError());
  dc_log(DC_LOG_ERROR, "TTF_OpenFont: %s", SDL_GetError());
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
  layout_compute();
  return true;
}

/* Draw a 2-pixel-thick border using SDL_RenderFillRect so it survives
 * SDL_RenderSetLogicalSize scaling (SDL_RenderDrawRect is 1 logical pixel
 * wide and can disappear at sub-1:1 scale factors). */
void draw_rect_border(SDL_Renderer *r, SDL_Rect rect) {
  SDL_RenderFillRect(r, &(SDL_Rect){rect.x, rect.y, rect.w, 2});
  SDL_RenderFillRect(r, &(SDL_Rect){rect.x, rect.y + rect.h - 2, rect.w, 2});
  SDL_RenderFillRect(r, &(SDL_Rect){rect.x, rect.y, 2, rect.h});
  SDL_RenderFillRect(r, &(SDL_Rect){rect.x + rect.w - 2, rect.y, 2, rect.h});
}

/* Draw a raised 3D border of `thickness` pixels outside `rect`.
 * Top/left edges are highlighted (light), bottom/right are shadowed (dark),
 * with each ring graduating from medium-gray at the outer edge to
 * bright/dark at the inner edge adjacent to the panel. */
void draw_3d_border(SDL_Renderer *r, SDL_Rect rect, int thickness) {
  for (int i = 0; i < thickness; i++) {
    int e = thickness - 1 - i; /* 0 at innermost ring */
    SDL_Rect ring = {rect.x - e - 1, rect.y - e - 1, rect.w + (e + 1) * 2, rect.h + (e + 1) * 2};

    float t = (float)i / (float)(thickness - 1); /* 0=outermost, 1=innermost */

    /* top + left highlight: medium-gray outer → bright inner */
    uint8_t hi = (uint8_t)(140.0f + t * 80.0f);
    SDL_SetRenderDrawColor(r, hi, hi, hi, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){ring.x, ring.y, ring.w, 1});
    SDL_RenderFillRect(r, &(SDL_Rect){ring.x, ring.y, 1, ring.h});

    /* bottom + right shadow: medium-gray outer → dark gray inner */
    uint8_t sh = (uint8_t)(110.0f - t * 40.0f);
    SDL_SetRenderDrawColor(r, sh, sh, sh, 255);
    SDL_RenderFillRect(r, &(SDL_Rect){ring.x, ring.y + ring.h - 1, ring.w, 1});
    SDL_RenderFillRect(r, &(SDL_Rect){ring.x + ring.w - 1, ring.y, 1, ring.h});
  }
}

void gfx_fill_circle(SDL_Renderer *r, int cx, int cy, int radius) {
  for (int y = -radius; y <= radius; y++) {
    float dy = (float)y / (float)radius;
    float inside = 1.0f - dy * dy;
    if (inside < 0.0f)
      continue;
    int x = (int)(radius * sqrtf(inside) + 0.5f);
    SDL_RenderDrawLine(r, cx - x, cy + y, cx + x, cy + y);
  }
}
