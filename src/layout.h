/*
 layout.h
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

#ifndef __LAYOUT_H
#define __LAYOUT_H

#include <SDL2/SDL.h>
#include "types.h"

/* Radius of the circular countdown timer */
#define CIRCLE_TIMER_R 50

/* Status message panel geometry */
#define MSG_PANEL_X_OFFSET 30
#define MSG_PANEL_W        420
#define MSG_PANEL_PAD_X    8
#define MSG_PANEL_PAD_Y    6

/* Number of scrolling status message lines shown in the panel */
#define SIZEOF_STATUS_MSGS 16

typedef struct {
  SDL_Point player_pos[MAX_PLAYERS]; /* top-left origin of each player's card row */
  SDL_Point timer;                   /* center of the circular countdown timer */
  SDL_Point table_center;            /* center of the table (pot coin landing point) */
  SDL_Rect  msg_panel;               /* status message panel rect */
  int       msg_panel_right;         /* x of right edge of msg_panel */
  int       action_btn_x;            /* left margin for action / amount buttons */
  int       status_line_h;           /* TTF line height of FONT_STATUS_MSG */
} GameLayout_t;

extern GameLayout_t g_layout;

/* Call once after fonts are open and g_viewport is set. */
void layout_init(int status_font_line_h);

/* Recompute all positions from g_viewport / g_center.
 * Called by layout_init and again by toggle_fullscreen. */
void layout_compute(void);

#endif
