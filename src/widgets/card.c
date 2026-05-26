/*
 widgets/card.c
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
#include <stdio.h>
#include <stdlib.h>

#include "card.h"
#include "globals.h"
#include "graphics.h"
#include "util.h"

// Build fails using gcc on Ubuntu 24.04 (and maybe others) without this
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Card back pattern/color selection
typedef struct {
  SDL_Color base_color;
  SDL_Color border_color;
  SDL_Color pattern_color;
  int pattern_type; // 0: crosshatch, 1: dots, 2: diagonal stripes, 3: grid
} CardBackStyle_t;

// pattern_type comments: 0: crosshatch, 1: dots, 2: diagonal stripes, 3: grid,
//                        4: diamond, 5: lava lamp, 6: sunset, 7: horse, 8: ocean waves
static CardBackStyle_t card_back_styles[] = {
    {{0, 0, 128, 255}, {255, 255, 255, 255}, {200, 200, 255, 255}, 0},   // blue crosshatch
    {{128, 0, 0, 255}, {255, 255, 255, 255}, {255, 200, 200, 255}, 0},   // red crosshatch
    {{128, 0, 0, 255}, {255, 255, 255, 255}, {255, 200, 200, 255}, 1},   // red dots
    {{128, 128, 0, 255}, {255, 255, 255, 255}, {255, 255, 200, 255}, 1}, // yellow dots
    {{0, 0, 128, 255}, {255, 255, 255, 255}, {200, 200, 255, 255}, 2},   // blue diagonal stripes
    {{128, 64, 0, 255}, {255, 255, 255, 255}, {255, 200, 128, 255}, 2},  // orange diagonal stripes
    {{128, 128, 0, 255}, {255, 255, 255, 255}, {255, 255, 200, 255}, 3}, // yellow grid
    {{128, 0, 128, 255},
     {255, 255, 255, 255},
     {255, 200, 255, 255},
     4},                                                 // purple with light stripes
    {{0, 0, 0, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, 5}, // lava lamp (animated)
    {{0, 0, 0, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, 6}, // sunset (animated)
    {{0, 0, 0, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, 7}, // horse walking (animated)
    {{0, 0, 0, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, 8}, // ocean waves (animated)
};

static int selected_card_back = -1;

void card_widget_select_back_for_game(void) {
  selected_card_back = pcg32_boundedrand_r(&rng, ARRAY_SIZE(card_back_styles));
}

static void draw_card_back_pattern(SDL_Renderer *renderer, SDL_Rect *card_rect) {
  const int style_idx = selected_card_back >= 0 ? selected_card_back : 0;
  CardBackStyle_t style = card_back_styles[style_idx];

  // Fill card with base color
  SDL_SetRenderDrawColor(renderer, style.base_color.r, style.base_color.g, style.base_color.b,
                         style.base_color.a);
  SDL_RenderFillRect(renderer, card_rect);

  // Draw border
  SDL_SetRenderDrawColor(renderer, style.border_color.r, style.border_color.g, style.border_color.b,
                         style.border_color.a);
  SDL_RenderDrawRect(renderer, card_rect);

  SDL_SetRenderDrawColor(renderer, style.pattern_color.r, style.pattern_color.g,
                         style.pattern_color.b, style.pattern_color.a);
  int spacing = 8;
  switch (style.pattern_type) {
  case 0: // crosshatch
    for (int y = 0; y < card_rect->h; y += spacing) {
      for (int x = 0; x < card_rect->w; x += spacing) {
        SDL_RenderDrawLine(renderer, card_rect->x + x, card_rect->y, card_rect->x,
                           card_rect->y + y);
      }
    }
    for (int y = 0; y < card_rect->h; y += spacing) {
      for (int x = 0; x < card_rect->w; x += spacing) {
        SDL_RenderDrawLine(renderer, card_rect->x + x, card_rect->y + card_rect->h,
                           card_rect->x + card_rect->w, card_rect->y + y);
      }
    }
    break;
  case 1: // dots
    for (int y = spacing; y < card_rect->h; y += spacing) {
      for (int x = spacing; x < card_rect->w; x += spacing) {
        SDL_Rect dot = {card_rect->x + x, card_rect->y + y, 2, 2};
        SDL_RenderFillRect(renderer, &dot);
      }
    }
    break;
  case 2: { // light green diagonal stripes (strictly inside border)
    // Offset by 1 to stay inside the border
    int left = card_rect->x + 1;
    int top = card_rect->y + 1;
    int right = card_rect->x + card_rect->w - 2;
    int bottom = card_rect->y + card_rect->h - 2;
    int w = right - left;
    int h = bottom - top;
    for (int x = -h; x <= w; x += spacing) {
      int x1 = left + (x < 0 ? 0 : x);
      int y1 = top + (x < 0 ? -x : 0);
      int x2 = left + (x + h <= w ? x + h : w);
      int y2 = top + (x + h <= w ? h : h - (x + h - w));
      SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
    break;
  }
  case 3: // grid
    for (int y = 0; y < card_rect->h; y += spacing) {
      SDL_RenderDrawLine(renderer, card_rect->x, card_rect->y + y, card_rect->x + card_rect->w,
                         card_rect->y + y);
    }
    for (int x = 0; x < card_rect->w; x += spacing) {
      SDL_RenderDrawLine(renderer, card_rect->x + x, card_rect->y, card_rect->x + x,
                         card_rect->y + card_rect->h);
    }
    break;
  case 4: { // purple with diamond grid (criss-cross)
    int left = card_rect->x + 1;
    int top = card_rect->y + 1;
    int right = card_rect->x + card_rect->w - 2;
    int bottom = card_rect->y + card_rect->h - 2;
    int w = right - left;
    int h = bottom - top;
    // Diagonal lines: top-left to bottom-right
    for (int x = -h; x <= w; x += spacing) {
      int x1 = left + (x < 0 ? 0 : x);
      int y1 = top + (x < 0 ? -x : 0);
      int x2 = left + (x + h <= w ? x + h : w);
      int y2 = top + (x + h <= w ? h : h - (x + h - w));
      SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
    // Diagonal lines: top-right to bottom-left
    for (int x = 0; x <= w + h; x += spacing) {
      int x1 = left + (x <= w ? x : w);
      int y1 = top + (x <= w ? 0 : x - w);
      int x2 = left + (x - h >= 0 ? x - h : 0);
      int y2 = top + (x - h >= 0 ? h : x);
      SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
    break;
  }
  case 5: { // lava lamp (animated)
    float t = (float)SDL_GetTicks() * 0.001f;

    // Dark purple background
    SDL_SetRenderDrawColor(renderer, 18, 0, 35, 255);
    SDL_RenderFillRect(renderer, card_rect);
    SDL_SetRenderDrawColor(renderer, 70, 0, 90, 255);
    SDL_RenderDrawRect(renderer, card_rect);

    SDL_RenderSetClipRect(renderer, card_rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    static const struct {
      float speed, phase_y, phase_x;
      Uint8 r, g, b;
      int radius;
    } blobs[] = {
        {0.40f, 0.00f, 1.30f, 220, 55, 0, 10},  {0.65f, 1.80f, 0.70f, 200, 130, 0, 9},
        {0.30f, 3.50f, 2.10f, 170, 0, 55, 11},  {0.75f, 0.90f, 4.20f, 240, 75, 0, 8},
        {0.50f, 2.70f, 3.00f, 155, 15, 80, 10},
    };

    int cx0 = card_rect->x + card_rect->w / 2;
    int cy0 = card_rect->y + card_rect->h / 2;
    int hy = card_rect->h / 2 - 4;
    int hx = card_rect->w / 5;

    for (int i = 0; i < 5; i++) {
      float ay = t * blobs[i].speed + blobs[i].phase_y;
      float ax = t * blobs[i].speed * 0.4f + blobs[i].phase_x;
      int cx = cx0 + (int)(sinf(ax) * hx);
      int cy = cy0 + (int)(sinf(ay) * hy);
      int r = blobs[i].radius;
      SDL_SetRenderDrawColor(renderer, blobs[i].r, blobs[i].g, blobs[i].b, 210);
      for (int dy = -r; dy <= r; dy++) {
        float inside = (float)(r * r - dy * dy);
        if (inside < 0.0f)
          continue;
        int dx = (int)(sqrtf(inside) + 0.5f);
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
      }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer, NULL);
    break;
  }
  case 6: { // sunset (animated) — 30-second repeating arc
    float t = (float)SDL_GetTicks() * 0.001f;
    float ph = fmodf(t, 30.0f) / 30.0f; // 0..1 over 30 seconds

    // Sky color: bright blue -> sunset orange -> night
    Uint8 sky_r, sky_g, sky_b;
    if (ph < 0.55f) {
      float p = ph / 0.55f;
      sky_r = (Uint8)(70 + p * 120);
      sky_g = (Uint8)(130 + p * 10);
      sky_b = (Uint8)(210 - p * 40);
    } else if (ph < 0.75f) {
      float p = (ph - 0.55f) / 0.20f;
      sky_r = (Uint8)(190 + p * 30); // peak 220, not 255
      sky_g = (Uint8)(140 - p * 60); // peak 80, stays orange not red
      sky_b = (Uint8)(170 - p * 160);
    } else {
      float p = (ph - 0.75f) / 0.25f;
      if (p > 1.0f)
        p = 1.0f;
      sky_r = (Uint8)(64 * (1.0f - p) + 5 * p);
      sky_g = (Uint8)(40 * (1.0f - p) + 5 * p);
      sky_b = (Uint8)(10 * (1.0f - p) + 20 * p);
    }

    SDL_SetRenderDrawColor(renderer, sky_r, sky_g, sky_b, 255);
    SDL_RenderFillRect(renderer, card_rect);
    SDL_SetRenderDrawColor(renderer, sky_r / 2, sky_g / 2, sky_b / 2, 255);
    SDL_RenderDrawRect(renderer, card_rect);

    SDL_RenderSetClipRect(renderer, card_rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Clouds: tinted by sky, drift left-to-right, fade out at sunset
    if (ph < 0.80f) {
      float cf = ph < 0.65f ? 1.0f : (0.80f - ph) / 0.15f;
      Uint8 ca = (Uint8)(180 * cf);
      Uint8 cr = ph < 0.55f ? 240 : 255;
      Uint8 cg = ph < 0.55f ? 240 : (Uint8)(240 - (ph - 0.55f) / 0.25f * 100);
      Uint8 cb = ph < 0.55f ? 240 : (Uint8)(240 - (ph - 0.55f) / 0.25f * 220);
      static const struct {
        float speed, phase;
        int dy_off, rx, ry;
      } clouds[] = {
          {0.025f, 0.10f, 8, 14, 4},
          {0.018f, 0.55f, 18, 9, 3},
      };
      SDL_SetRenderDrawColor(renderer, cr, cg, cb, ca);
      for (int i = 0; i < 2; i++) {
        float fx = fmodf(clouds[i].phase + t * clouds[i].speed, 1.0f);
        int cx = card_rect->x + (int)(fx * (card_rect->w + clouds[i].rx * 2)) - clouds[i].rx;
        int cy = card_rect->y + clouds[i].dy_off;
        for (int dy = -clouds[i].ry; dy <= clouds[i].ry; dy++) {
          float inside = 1.0f - (float)(dy * dy) / (float)(clouds[i].ry * clouds[i].ry);
          if (inside < 0.0f)
            continue;
          int dx = (int)(clouds[i].rx * sqrtf(inside) + 0.5f);
          SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
        }
      }
    }

    // Sun: arcs from top-left toward bottom-right, disappears behind horizon
    float sun_fx = 0.15f + ph * 0.70f;
    float sun_fy = ph;
    int sun_cx = card_rect->x + (int)(sun_fx * card_rect->w);
    int sun_cy = card_rect->y + 4 + (int)(sun_fy * (card_rect->h - 8));
    int sun_rad = 5;
    Uint8 sun_g = ph < 0.60f ? 230 : (Uint8)(230 - (ph - 0.60f) / 0.40f * 180);
    Uint8 sun_b = ph < 0.60f ? 80 : 0;
    SDL_SetRenderDrawColor(renderer, 255, sun_g, sun_b, 255);
    for (int dy = -sun_rad; dy <= sun_rad; dy++) {
      float inside = (float)(sun_rad * sun_rad - dy * dy);
      if (inside < 0.0f)
        continue;
      int dx = (int)(sqrtf(inside) + 0.5f);
      SDL_RenderDrawLine(renderer, sun_cx - dx, sun_cy + dy, sun_cx + dx, sun_cy + dy);
    }

    // Horizon: dark ground at bottom, 7px tall on left edge, 13px on right,
    // drawn on top of everything so the sun sinks behind it naturally.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int row = 0; row < 13; row++) {
      int y = card_rect->y + card_rect->h - 1 - row;
      int x_start = row <= 7 ? 0 : (row - 7) * card_rect->w / 6;
      SDL_RenderDrawLine(renderer, card_rect->x + x_start, y, card_rect->x + card_rect->w - 1, y);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Stars: fade in after sunset
    if (ph > 0.78f) {
      float sf = (ph - 0.78f) / 0.10f;
      if (sf > 1.0f)
        sf = 1.0f;
      Uint8 sa = (Uint8)(220 * sf);
      SDL_SetRenderDrawColor(renderer, 255, 255, 200, sa);
      static const SDL_Point stars[] = {
          {5, 5}, {20, 8}, {35, 3}, {55, 12}, {70, 6}, {12, 18}, {45, 22}, {62, 15}, {28, 25},
      };
      for (int i = 0; i < 9; i++)
        SDL_RenderDrawPoint(renderer, card_rect->x + stars[i].x, card_rect->y + stars[i].y);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer, NULL);
    break;
  }
  case 7: { // horse walking (animated)
    float t = (float)SDL_GetTicks() * 0.001f;

    // Sky
    SDL_SetRenderDrawColor(renderer, 120, 185, 230, 255);
    SDL_RenderFillRect(renderer, card_rect);

    SDL_RenderSetClipRect(renderer, card_rect);

    // Grass strip — bottom 12 rows
    int grass_top = card_rect->y + card_rect->h - 12;
    SDL_SetRenderDrawColor(renderer, 55, 140, 45, 255);
    SDL_Rect grass = {card_rect->x, grass_top, card_rect->w, 12};
    SDL_RenderFillRect(renderer, &grass);
    // Darker grass tufts
    SDL_SetRenderDrawColor(renderer, 35, 100, 28, 255);
    for (int gx = 0; gx < card_rect->w; gx += 7) {
      SDL_RenderDrawLine(renderer, card_rect->x + gx, grass_top, card_rect->x + gx, grass_top - 2);
    }

    // Horse position: walks left-to-right, wrapping
    int wrap_w = card_rect->w + 44;
    float walk_speed = 14.0f; // pixels per second
    int horse_cx = card_rect->x - 22 + (int)fmodf(t * walk_speed, (float)wrap_w);
    int horse_y = grass_top - 8; // bottom of body

    // Walk cycle: two leg pairs in opposite phase
    float stride = fmodf(t * 4.5f, (float)(2 * M_PI));
    int p1 = (int)(5.0f * sinf(stride));         // near pair
    int p2 = (int)(5.0f * sinf(stride + 3.14f)); // far pair

    // Far legs (drawn behind body — darker brown)
    SDL_SetRenderDrawColor(renderer, 72, 45, 12, 255);
    // far front leg
    SDL_RenderDrawLine(renderer, horse_cx + 5, horse_y + 8, horse_cx + 5 + p2, horse_y + 8 + 8);
    // far hind leg
    SDL_RenderDrawLine(renderer, horse_cx - 5, horse_y + 8, horse_cx - 5 - p2, horse_y + 8 + 8);

    // Body
    SDL_SetRenderDrawColor(renderer, 105, 68, 28, 255);
    SDL_Rect body = {horse_cx - 12, horse_y, 24, 9};
    SDL_RenderFillRect(renderer, &body);
    // Highlight stripe along top of body
    SDL_SetRenderDrawColor(renderer, 140, 95, 48, 255);
    SDL_RenderDrawLine(renderer, horse_cx - 11, horse_y + 1, horse_cx + 11, horse_y + 1);

    // Neck: 4 lines going up-right from front of body
    SDL_SetRenderDrawColor(renderer, 105, 68, 28, 255);
    for (int ni = 0; ni < 4; ni++) {
      SDL_RenderDrawLine(renderer, horse_cx + 10 + ni, horse_y + 7 - ni, horse_cx + 14 + ni,
                         horse_y - 5 - ni);
    }

    // Head
    SDL_Rect head = {horse_cx + 14, horse_y - 9, 8, 5};
    SDL_RenderFillRect(renderer, &head);

    // Mane: dark strip along neck
    SDL_SetRenderDrawColor(renderer, 38, 18, 4, 255);
    for (int ni = 0; ni < 4; ni++) {
      SDL_RenderDrawPoint(renderer, horse_cx + 11 + ni, horse_y + 6 - ni);
    }

    // White blaze on nose
    SDL_SetRenderDrawColor(renderer, 230, 225, 215, 255);
    SDL_RenderDrawLine(renderer, horse_cx + 20, horse_y - 8, horse_cx + 20, horse_y - 6);

    // Eye
    SDL_SetRenderDrawColor(renderer, 15, 10, 5, 255);
    SDL_RenderDrawPoint(renderer, horse_cx + 16, horse_y - 7);

    // Nostril
    SDL_RenderDrawPoint(renderer, horse_cx + 21, horse_y - 5);

    // Near legs (drawn in front of body)
    SDL_SetRenderDrawColor(renderer, 105, 68, 28, 255);
    // near front leg
    SDL_RenderDrawLine(renderer, horse_cx + 6, horse_y + 8, horse_cx + 6 + p1, horse_y + 8 + 8);
    // near hind leg
    SDL_RenderDrawLine(renderer, horse_cx - 4, horse_y + 8, horse_cx - 4 - p1, horse_y + 8 + 8);

    // White sock on near front leg bottom
    SDL_SetRenderDrawColor(renderer, 210, 205, 195, 255);
    int sock_bx = horse_cx + 6 + p1;
    int sock_by = horse_y + 16;
    SDL_RenderDrawLine(renderer, sock_bx, sock_by, sock_bx, sock_by + 1);

    // Tail: swishing at the rear
    SDL_SetRenderDrawColor(renderer, 38, 18, 4, 255);
    float tail_sw = sinf(t * 1.8f) * 5.0f;
    SDL_RenderDrawLine(renderer, horse_cx - 12, horse_y + 2, horse_cx - 18 + (int)tail_sw,
                       horse_y + 11);
    SDL_RenderDrawLine(renderer, horse_cx - 18 + (int)tail_sw, horse_y + 11,
                       horse_cx - 20 + (int)(tail_sw * 0.6f), horse_y + 16);

    SDL_RenderSetClipRect(renderer, NULL);
    break;
  }
  case 8: { // ocean waves — angled top-down view
    // Perspective: y=0 is the far horizon, y=h-1 is the near water surface.
    // Wave bands compress toward the top (quadratic depth mapping).
    float t = (float)SDL_GetTicks() * 0.001f;
    SDL_RenderSetClipRect(renderer, card_rect);

    for (int py = 0; py < card_rect->h; py++) {
      float persp = (float)py / (float)(card_rect->h - 1); // 0=far, 1=near
      float depth = persp * persp * 15.0f; // world depth units (quadratic = perspective)

      // Base water color: dark blue at horizon, more teal at the near edge
      float base_r = 8.0f + persp * 25.0f;
      float base_g = 55.0f + persp * 45.0f;
      float base_b = 130.0f + persp * 40.0f;

      for (int px = 0; px < card_rect->w; px++) {
        // Diagonal wave fronts: combine depth and x so crests run SW to NE,
        // traveling toward the bottom-left (toward the viewer)
        float wave = sinf(depth * 2.8f + (float)px * 0.25f - t * 2.2f);

        float r = base_r, g = base_g, b = base_b;

        if (wave > 0.65f) {
          // Whitecap / foam
          float f = (wave - 0.65f) / 0.35f;
          r = base_r + f * (225.0f - base_r);
          g = base_g + f * (235.0f - base_g);
          b = base_b + f * (245.0f - base_b);
        } else if (wave > 0.1f) {
          // Crest face: brighter teal
          float f = (wave - 0.1f) / 0.55f;
          r = base_r + f * 10.0f;
          g = base_g + f * 35.0f;
          b = base_b + f * 20.0f;
        } else if (wave < -0.4f) {
          // Trough: deeper, darker
          float f = (-wave - 0.4f) / 0.6f;
          r = base_r * (1.0f - f * 0.35f);
          g = base_g * (1.0f - f * 0.35f);
          b = base_b * (1.0f - f * 0.15f);
        }

        SDL_SetRenderDrawColor(renderer, (Uint8)r, (Uint8)g, (Uint8)b, 255);
        SDL_RenderDrawPoint(renderer, card_rect->x + px, card_rect->y + py);
      }
    }

    SDL_RenderSetClipRect(renderer, NULL);
    break;
  }
  default:
    break;
  }
}

static void card_widget_render(UIWidget_t *w) {
  CardWidget_t *cw = (CardWidget_t *)w;
  SDL_Renderer *renderer = g_sdl_context->renderer;

  if (cw->is_null)
    return;
  if (cw->is_back) {
    draw_card_back_pattern(renderer, &w->rect);
    return;
  }

  // Draw white card box
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderFillRect(renderer, &w->rect);

  // Highlight winning cards: gold tint + 3D bevel border (5× thickness).
  if (cw->is_winning) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 200, 0, 110); // translucent gold fill
    SDL_RenderFillRect(renderer, &w->rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    /* Raised-bevel effect drawn outside the card rect: bright gold on
     * top/left edges, dark brown on bottom/right edges.  t=0 is the ring
     * immediately adjacent to the card boundary; t=B-1 is the outermost. */
    const int B = 4;
    for (int t = 0; t < B; t++) {
      int x1 = w->rect.x - t - 1;
      int y1 = w->rect.y - t - 1;
      int x2 = w->rect.x + w->rect.w + t;
      int y2 = w->rect.y + w->rect.h + t;

      SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); // bright gold — highlight
      SDL_RenderDrawLine(renderer, x1, y1, x2, y1);       // top
      SDL_RenderDrawLine(renderer, x1, y1, x1, y2);       // left

      SDL_SetRenderDrawColor(renderer, 130, 80, 0, 255); // dark brown — shadow
      SDL_RenderDrawLine(renderer, x1, y2, x2, y2);      // bottom
      SDL_RenderDrawLine(renderer, x2, y1, x2, y2);      // right
    }
  }

  // Highlight hovered card for the local player (draw after card background)
  if (w->hovered && cw->my_card) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 128, 96); // translucent yellow
    SDL_RenderFillRect(renderer, &w->rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  }

  if (w->selected)
    mark_selected(renderer, &w->rect);

  // Draw card border
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderDrawRect(renderer, &w->rect);

  /* Cached text texture — only re-rendered when the card's face or color
   * changes.  Before this cache, TTF_RenderUTF8_Blended +
   * SDL_CreateTextureFromSurface ran every frame for every card and
   * accounted for the lion's share of the 32k allocations/sec heaptrack
   * caught during gameplay. */
  if (!card_widget_text_cache_valid(cw)) {
    if (cw->cached_text_texture) {
      SDL_DestroyTexture(cw->cached_text_texture);
      cw->cached_text_texture = NULL;
    }
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(cw->font, cw->text, cw->textColor);
    if (!textSurface) {
      fprintf(stderr, "TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
      exit(EXIT_FAILURE);
    }
    cw->cached_text_texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!cw->cached_text_texture) {
      fprintf(stderr, "SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
      SDL_FreeSurface(textSurface);
      exit(EXIT_FAILURE);
    }
    cw->cached_text_w = textSurface->w;
    cw->cached_text_h = textSurface->h;
    SDL_FreeSurface(textSurface);
    snprintf(cw->cached_text, sizeof(cw->cached_text), "%s", cw->text);
    cw->cached_text_color = cw->textColor;
    card_widget_text_cache_misses++;
  }

  SDL_Rect textRect = {w->rect.x + (g_layout_cfg.card_w - cw->cached_text_w) / 2,
                       w->rect.y + (g_layout_cfg.card_h - cw->cached_text_h) / 2,
                       cw->cached_text_w, cw->cached_text_h};
  SDL_RenderCopy(renderer, cw->cached_text_texture, NULL, &textRect);
}

unsigned long card_widget_text_cache_misses = 0;

bool card_widget_text_cache_valid(const CardWidget_t *cw) {
  if (!cw->cached_text_texture)
    return false;
  if (strcmp(cw->cached_text, cw->text) != 0)
    return false;
  if (cw->cached_text_color.r != cw->textColor.r ||
      cw->cached_text_color.g != cw->textColor.g ||
      cw->cached_text_color.b != cw->textColor.b ||
      cw->cached_text_color.a != cw->textColor.a)
    return false;
  return true;
}

static void card_widget_destroy(UIWidget_t *w) {
  CardWidget_t *cw = (CardWidget_t *)w;
  if (cw->cached_text_texture) {
    SDL_DestroyTexture(cw->cached_text_texture);
    cw->cached_text_texture = NULL;
  }
  free(w);
}

void card_widget_init(CardWidget_t *cw, TTF_Font *font) {
  cw->font = font;
  cw->base.rect.w = g_layout_cfg.card_w;
  cw->base.rect.h = g_layout_cfg.card_h;
  cw->base.render = card_widget_render;
  cw->base.destroy = card_widget_destroy;
  cw->cached_text_texture = NULL;
  cw->cached_text_w = cw->cached_text_h = 0;
  cw->cached_text[0] = '\0';
  cw->cached_text_color = (SDL_Color){0};
}
