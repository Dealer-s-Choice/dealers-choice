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

#include "layout_config.h"
#include "types.h"
#include <SDL2/SDL.h>

typedef struct {
  SDL_Point player_pos[MAX_PLAYERS]; /* fixed ring anchors (opponent seats) A0..A4 */
  SDL_Point local_seat;              /* local player's hand: center x, row-top y (bottom-center) */
  SDL_Point timer;                   /* center of the circular countdown timer */
  SDL_Point table_center;            /* center of the table (pot coin landing point) */
  int pot_radius;                    /* max pot-coin scatter radius (collision-bounded) */
  SDL_Rect msg_panel;                /* status message panel rect */
  int msg_panel_right;               /* x of right edge of msg_panel */
  int action_btn_x;                  /* left margin for action / amount buttons */
  int status_line_h;                 /* TTF line height of FONT_STATUS_MSG */

  struct {
    int title_x;           /* x of screen title widget (both screens) */
    int title_y;           /* y of screen title widget (both screens) */
    int margin_x;          /* left column x: connect inputs and settings x_left */
    int connect_btn_y;     /* y of Connect / Settings nav buttons */
    int connect_host_y;    /* y of host input field */
    int quit_y;            /* y of top-right X quit button */
    int settings_x_right;  /* right column x for settings grid */
    int settings_x_third;  /* third column x: Hotkeys button */
    int settings_row_y[3]; /* y of each settings row (label baseline) */
    int settings_save_y;   /* y of Save / Load Defaults buttons */
    int back_img_x;        /* x of settings back-arrow image */
    int back_img_y;        /* y of settings back-arrow image */
    int links_center_x;    /* horizontal center for link widgets */
  } menu;

  struct {
    int waiting_y; /* y of "Waiting for players/dealer" status text */
    int kick_x;    /* x of Kick / Ban admin buttons */
    int kick_y;    /* y of Kick / Ban admin buttons */
  } lobby;
} GameLayout_t;

extern GameLayout_t g_layout;

/* GUI viewport state: the logical render rect and its center. Set during
 * renderer init and on fullscreen/resize; read by the layout calculations. */
extern SDL_Rect g_viewport;
extern SDL_Point g_center;

/* Call once after fonts are open and g_viewport is set. */
void layout_init(int status_font_line_h);

/* Recompute all positions from g_viewport / g_center.
 * Called by layout_init and again by toggle_fullscreen. */
void layout_compute(void);

/* ---- Geometry helpers (anchor-relative placement) ----------------------- */

/* Which point of a w*h box is pinned to the reference point.
 * column = a % 3 (0 left, 1 center, 2 right); row = a / 3 (0 top, 1 mid, 2 bot). */
typedef enum {
  ANCHOR_TOP_LEFT,
  ANCHOR_TOP_CENTER,
  ANCHOR_TOP_RIGHT,
  ANCHOR_MID_LEFT,
  ANCHOR_CENTER,
  ANCHOR_MID_RIGHT,
  ANCHOR_BOTTOM_LEFT,
  ANCHOR_BOTTOM_CENTER,
  ANCHOR_BOTTOM_RIGHT,
} Anchor_t;

/* Box of size w*h whose `a` anchor sits at ref, then shifted by (dx, dy). */
SDL_Rect rect_anchored(SDL_Point ref, int w, int h, Anchor_t a, int dx, int dy);

/* Fill out[game_id] with each player's card-row top-left origin, rotated so the
 * local player is bottom-center (g_layout.local_seat) and opponents fill the
 * fixed ring clockwise. local_row_w centers the local player's row. */
void layout_seats_for(SDL_Point out[MAX_PLAYERS], int local_id, int local_row_w);

#endif
