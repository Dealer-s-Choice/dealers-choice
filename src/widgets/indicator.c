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

  // Drop shadow (drawn before clip is set so it appears behind the oval)
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 90);
  draw_filled_ellipse(r, ind->cx + 5, ind->cy + 6, ind->rx + 2, ind->ry + 2);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  // Clip all drawing to the indicator's bounding box
  SDL_Rect clip = {ind->cx - ind->rx, ind->cy - ind->ry, ind->rx * 2, ind->ry * 2};
  SDL_RenderSetClipRect(r, &clip);

  // White base fill
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  draw_filled_ellipse(r, ind->cx, ind->cy, ind->rx, ind->ry);

  // Swirling mist layers tinted by bg_color
  static const struct {
    float speed, phase, fx, fy;
    Uint8 alpha;
  } layers[] = {
      {0.40f, 0.00f, 0.45f, 0.35f, 180}, {0.55f, 1.26f, 0.40f, 0.45f, 160},
      {0.30f, 2.51f, 0.50f, 0.30f, 150}, {0.65f, 3.77f, 0.35f, 0.40f, 170},
      {0.45f, 5.03f, 0.42f, 0.38f, 155}, {0.35f, 0.63f, 0.48f, 0.32f, 130},
      {0.60f, 4.40f, 0.38f, 0.42f, 145},
  };

  SDL_Color base = ind->bg_color;
  float t = SDL_GetTicks() * 0.001f;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < 7; i++) {
    float a = t * layers[i].speed + layers[i].phase;
    int ox = (int)(sinf(a) * ind->rx * layers[i].fx);
    int oy = (int)(cosf(a * 0.7f) * ind->ry * layers[i].fy);
    SDL_SetRenderDrawColor(r, base.r, base.g, base.b, layers[i].alpha);
    draw_filled_ellipse(r, ind->cx + ox, ind->cy + oy, ind->rx, ind->ry);
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  SDL_RenderSetClipRect(r, NULL);

  // render text centered in oval
  ind->text->base.rect.x = ind->cx - ind->text->base.rect.w / 2;
  ind->text->base.rect.y = ind->cy - ind->text->base.rect.h / 2;
  ui_widget_render(&ind->text->base);
}

static Indicator_t *indicator_init(const char *text, TTF_Font *font,
                                   SDL_Color bg_color, SDL_Color fg_color) {
  SDL_Renderer *renderer = g_sdl_context->renderer;
  if (!renderer || !text || !font)
    return NULL;

  Indicator_t *ind = calloc(1, sizeof(*ind));
  if (!ind)
    return NULL;

  ind->renderer = renderer;
  ind->bg_color = bg_color;

  ind->text = text_widget_create(text, font, fg_color);
  if (!ind->text) {
    free(ind);
    return NULL;
  }

  int text_w = ind->text->base.rect.w;
  int text_h = ind->text->base.rect.h;
  int pad_x = text_h;
  int pad_y = text_h / 3;
  ind->base.rect.w = text_w + pad_x * 2;
  ind->base.rect.h = text_h + pad_y * 2;

  ind->rx = ind->base.rect.w / 2;
  ind->ry = ind->base.rect.h / 2;

  ind->base.render = indicator_render;
  ind->base.destroy = text_wrapper_destroy;

  return ind;
}

Indicator_t *create_indicator(const char *text, TTF_Font *font, EColorName_t bg_color,
                              EColorName_t fg_color) {
  return indicator_init(text, font, get_color(bg_color), get_color(fg_color));
}

Indicator_t *create_indicator_colored(const char *text, TTF_Font *font,
                                      SDL_Color bg_color, SDL_Color fg_color) {
  return indicator_init(text, font, bg_color, fg_color);
}
