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

#include <math.h>

#include "indicator.h"
#include "text.h"
#include "ui_widget.h"

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

// render function (non-static so we can assign pointer)
void indicator_render(UIWidget_t *w) {
  Indicator_t *ind = (Indicator_t *)w;
  SDL_Renderer *r = ind->renderer;

  // Clip all drawing to the indicator's bounding box
  SDL_Rect clip = {ind->cx - ind->rx, ind->cy - ind->ry, ind->rx * 2, ind->ry * 2};
  SDL_RenderSetClipRect(r, &clip);

  // White base fill
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  draw_filled_ellipse(r, ind->cx, ind->cy, ind->rx, ind->ry);

  // Swirling mist layers: white blending into pale sandy orange
  static const struct {
    float speed, phase, fx, fy;
    Uint8 red, green, blue, alpha;
  } layers[] = {
    { 0.40f, 0.00f, 0.45f, 0.35f, 255, 180, 100, 180 }, // deep amber
    { 0.55f, 1.26f, 0.40f, 0.45f, 255, 140,  60, 160 }, // burnt orange
    { 0.30f, 2.51f, 0.50f, 0.30f, 255, 220, 160, 150 }, // pale peach
    { 0.65f, 3.77f, 0.35f, 0.40f, 255, 160,  80, 170 }, // warm orange
    { 0.45f, 5.03f, 0.42f, 0.38f, 255, 200, 130, 155 }, // sandy
    { 0.35f, 0.63f, 0.48f, 0.32f, 255, 240, 200, 130 }, // cream
    { 0.60f, 4.40f, 0.38f, 0.42f, 255, 120,  40, 145 }, // deep rust
  };

  float t = SDL_GetTicks() * 0.001f;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < 7; i++) {
    float a = t * layers[i].speed + layers[i].phase;
    int ox = (int)(sinf(a)        * ind->rx * layers[i].fx);
    int oy = (int)(cosf(a * 0.7f) * ind->ry * layers[i].fy);
    SDL_SetRenderDrawColor(r, layers[i].red, layers[i].green, layers[i].blue, layers[i].alpha);
    draw_filled_ellipse(r, ind->cx + ox, ind->cy + oy, ind->rx, ind->ry);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  SDL_RenderSetClipRect(r, NULL);

  // render text centered in oval
  ind->text->base.rect.x = ind->cx - ind->text->base.rect.w / 2;
  ind->text->base.rect.y = ind->cy - ind->text->base.rect.h / 2;
  ui_widget_render(&ind->text->base);
}

Indicator_t *create_indicator(const char *text, TTF_Font *font, EColorName_t bg_color,
                              EColorName_t fg_color) {
  SDL_Renderer *renderer = g_sdl_context->renderer;
  if (!renderer || !text || !font)
    return NULL;

  Indicator_t *ind = calloc(1, sizeof(*ind));
  if (!ind)
    return NULL;

  ind->renderer = renderer;
  ind->bg_color = get_color(bg_color);

  ind->text = text_widget_create(text, font, get_color(fg_color));
  if (!ind->text) {
    free(ind);
    return NULL;
  }

  // Compute indicator rectangle with padding
  int text_w = ind->text->base.rect.w;
  int text_h = ind->text->base.rect.h;
  int pad_x = text_h;     // horizontal padding
  int pad_y = text_h / 3; // vertical padding
  ind->base.rect.w = text_w + pad_x * 2;
  ind->base.rect.h = text_h + pad_y * 2;

  // Precompute oval radii
  ind->rx = ind->base.rect.w / 2;
  ind->ry = ind->base.rect.h / 2;

  // Assign callbacks
  ind->base.render = indicator_render;
  ind->base.destroy = text_wrapper_destroy;

  return ind;
}
