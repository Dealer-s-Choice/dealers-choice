/*
 diamond_button.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2026 Andy Alt
 Written by Claude (Opus 4.8) at Andy's direction.

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

#include <stdlib.h>

#include "diamond_button.h"
#include "globals_gui.h"
#include "graphics.h"

/* Small horizontal inset so the left/right gem points stop just short of the
 * cell side margins (Andy: "almost to the margin"). Top/bottom points run the
 * full box height. */
#define DIAMOND_X_INSET 3

static Uint8 clampb(int v) { return v < 0 ? 0 : (v > 255 ? 255 : (Uint8)v); }

/* Half-extents of the rhombus the button draws / hit-tests against, derived
 * from the bounding rect. Shared by render and hit-test so the two never drift
 * out of agreement. */
static void diamond_extents(const SDL_Rect *rect, int *cx, int *cy, int *halfW, int *halfH) {
  *halfW = rect->w / 2 - DIAMOND_X_INSET;
  if (*halfW < 1)
    *halfW = 1;
  *halfH = rect->h / 2;
  if (*halfH < 1)
    *halfH = 1;
  *cx = rect->x + rect->w / 2;
  *cy = rect->y + rect->h / 2;
}

bool diamond_button_hit(const DiamondButtonWidget_t *b, int px, int py) {
  if (!b)
    return false;
  int cx, cy, halfW, halfH;
  diamond_extents(&b->base.rect, &cx, &cy, &halfW, &halfH);
  /* Rhombus test: |dx|/halfW + |dy|/halfH <= 1. Scaled to integers
   * (multiply through by halfW*halfH) to avoid float rounding at the edge. */
  long dx = labs((long)px - cx);
  long dy = labs((long)py - cy);
  return dx * halfH + dy * halfW <= (long)halfW * halfH;
}

/* Fill the rhombus with horizontal scanlines, top point to bottom point. Each
 * scanline's half-width shrinks linearly toward the points; the fill colour
 * runs from a lit top to a darker bottom to read as a faceted gem. */
static void diamond_fill_gradient(SDL_Renderer *r, int cx, int cy, int halfW, int halfH,
                                  SDL_Color top, SDL_Color bot) {
  for (int y = -halfH; y <= halfH; y++) {
    /* line half-width at this row: full at the centre (y==0), 0 at the points */
    int hw = halfW - (halfW * abs(y)) / halfH;
    /* t: 0 at the top point, 1 at the bottom point */
    float t = (float)(y + halfH) / (float)(2 * halfH);
    Uint8 cr = clampb((int)(top.r + (bot.r - top.r) * t));
    Uint8 cg = clampb((int)(top.g + (bot.g - top.g) * t));
    Uint8 cb = clampb((int)(top.b + (bot.b - top.b) * t));
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDrawLine(r, cx - hw, cy + y, cx + hw, cy + y);
  }
}

static void diamond_button_render(UIWidget_t *w) {
  DiamondButtonWidget_t *b = (DiamondButtonWidget_t *)w;
  SDL_Renderer *r = g_sdl_context->renderer;

  int cx, cy, halfW, halfH;
  diamond_extents(&w->rect, &cx, &cy, &halfW, &halfH);

  const int boost = w->hovered ? 45 : 0;
  const SDL_Color c = {clampb(b->color.r + boost), clampb(b->color.g + boost),
                       clampb(b->color.b + boost), 255};

  /* Gem body: lighter top facet -> deeper bottom facet (the gradient sells the
   * "cut stone" look without needing per-vertex SDL_RenderGeometry). */
  const SDL_Color gem_top = {clampb(c.r + 70), clampb(c.g + 70), clampb(c.b + 70), 255};
  const SDL_Color gem_bot = {clampb(c.r - 35), clampb(c.g - 35), clampb(c.b - 35), 255};

  /* Outer dark rim first (slightly larger rhombus), then the gem inscribed one
   * pixel in, so the body sits in a thin dark outline. */
  SDL_SetRenderDrawColor(r, clampb(c.r / 3), clampb(c.g / 3), clampb(c.b / 3), 255);
  diamond_fill_gradient(r, cx, cy, halfW, halfH, (SDL_Color){clampb(c.r / 3), clampb(c.g / 3), clampb(c.b / 3), 255},
                        (SDL_Color){clampb(c.r / 4), clampb(c.g / 4), clampb(c.b / 4), 255});
  diamond_fill_gradient(r, cx, cy, halfW - 1, halfH - 1, gem_top, gem_bot);

  /* Bevel: light edges on the top-left two faces, dark edges on the
   * bottom-right two faces, drawn between the four points. */
  const int lx = cx - halfW, rx = cx + halfW, ty = cy - halfH, by = cy + halfH;
  SDL_SetRenderDrawColor(r, clampb(c.r + 110), clampb(c.g + 110), clampb(c.b + 110), 255);
  SDL_RenderDrawLine(r, lx, cy, cx, ty); /* left point -> top point */
  SDL_RenderDrawLine(r, cx, ty, rx, cy); /* top point  -> right point */
  SDL_SetRenderDrawColor(r, clampb(c.r - 60), clampb(c.g - 60), clampb(c.b - 60), 255);
  SDL_RenderDrawLine(r, rx, cy, cx, by); /* right point -> bottom point */
  SDL_RenderDrawLine(r, cx, by, lx, cy); /* bottom point -> left point */

  /* Specular gloss: a small bright wedge near the upper-left facet to catch the
   * eye as a polished highlight. */
  int gw = halfW / 3;
  int gh = halfH / 3;
  if (gw > 0 && gh > 0) {
    int gcx = cx - halfW / 4;
    int gcy = cy - halfH / 3;
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    for (int y = -gh; y <= gh; y++) {
      int hw = gw - (gw * abs(y)) / gh;
      SDL_RenderDrawLine(r, gcx - hw, gcy + y, gcx + hw, gcy + y);
    }
  }
}

static void diamond_button_destroy(UIWidget_t *w) { free(w); }

DiamondButtonWidget_t *diamond_button_create(int w, int h, SDL_Color color) {
  DiamondButtonWidget_t *b = calloc(1, sizeof(*b));
  if (!b)
    return NULL;
  b->color = color;
  b->base.rect.w = w;
  b->base.rect.h = h;
  b->base.render = diamond_button_render;
  b->base.destroy = diamond_button_destroy;
  return b;
}
