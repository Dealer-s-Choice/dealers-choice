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
#include "game.h"

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
      .bg_color = get_color(COLOR_WHITE),
      .fg_color = get_color(COLOR_BROWN),
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
