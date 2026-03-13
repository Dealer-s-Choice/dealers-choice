/*
 indicator.c
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

#include "indicator.h"

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
  SDL_Renderer *r = ind->renderer;

  SDL_SetRenderDrawColor(r, ind->bg_color.r, ind->bg_color.g, ind->bg_color.b, ind->bg_color.a);

  draw_filled_ellipse(r, ind->cx, ind->cy, ind->rx, ind->ry);

  SDL_RenderCopy(r, ind->text_tex, NULL, &ind->text_rect);
}

Indicator_t create_indicator(SDL_Renderer *renderer, const char *text, const Font_t *font) {
  Indicator_t ind = {.text = text,
                     .renderer = renderer,
                     .bg_color = get_color(COLOR_WHITE),
                     .fg_color = get_color(COLOR_BROWN),
                     .rect = {0},
                     .text_rect = {0},
                     .text_tex = NULL,
                     .font = font->fonts[FONT_BOLD],
                     .cx = 0,
                     .cy = 0,
                     .rx = 0,
                     .ry = 0};

  SDL_Surface *surf = TTF_RenderUTF8_Blended(ind.font, ind.text, ind.fg_color);
  if (!surf) {
    fprintf(stderr, "TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
    ind.rect.w = 40;
    ind.rect.h = 20;
    return ind;
  }

  ind.text_tex = SDL_CreateTextureFromSurface(renderer, surf);

  int text_w = surf->w;
  int text_h = surf->h;

  int PAD_X = text_h;
  int PAD_Y = text_h / 3;

  ind.rect.w = text_w + PAD_X * 2;
  ind.rect.h = text_h + PAD_Y * 2;

  ind.text_rect.w = text_w;
  ind.text_rect.h = text_h;

  SDL_FreeSurface(surf);

  return ind;
}

void destroy_indicator(Indicator_t *ind) {
  if (ind && ind->text_tex)
    SDL_DestroyTexture(ind->text_tex);
}
