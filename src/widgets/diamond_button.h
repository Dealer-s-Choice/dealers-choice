/*
 diamond_button.h
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2026 Andy Alt
 New-file scaffolding written by Claude (Opus 4.8) at Andy's direction.

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

#ifndef __DIAMOND_BUTTON_WIDGET_H
#define __DIAMOND_BUTTON_WIDGET_H

#include "ui_widget.h"

/* A card-suit DIAMOND ("gem") button with no label, drawn to fill its table
 * cell (a rotated square: left/right points near the cell side margins, top
 * point near the cell top, bottom point at the full cell bottom). Used for the
 * Connect cell in the server-list tables, replacing the small round button.
 *
 * The whole bounding box (base.rect) is reserved in the table layout, but only
 * the diamond region is the click target — see diamond_button_hit(). Callers
 * MUST hit-test with diamond_button_hit() rather than SDL_PointInRect, or
 * clicks in the empty triangular corners would falsely register.
 *
 * base.hovered brightens the gem (set it from the hit-test for true diamond-
 * shaped hover, not the bounding rect).
 *
 * Rendered without SDL_RenderGeometry (which needs SDL >= 2.0.18) — the gem fill
 * is horizontal scanlines with a top-to-bottom gradient, so it works on any
 * SDL 2.0.x the rest of the project targets. */
typedef struct {
  UIWidget_t base; /* base.rect = bounding box the diamond is inscribed in */
  SDL_Color color; /* base gem fill; shaded lighter at the top, darker at bottom */
} DiamondButtonWidget_t;

/* Create a diamond button sized w x h (the bounding box). The diamond's points
 * touch the mid-points of the four box edges; a small inset is applied inside
 * render so the left/right points don't quite reach the cell margins. */
DiamondButtonWidget_t *diamond_button_create(int w, int h, SDL_Color color);

/* True iff point (px,py) is inside the diamond region of `b` (not just its
 * bounding box). Uses the rhombus inequality |dx|/halfW + |dy|/halfH <= 1
 * about the box centre. */
bool diamond_button_hit(const DiamondButtonWidget_t *b, int px, int py);

#endif
