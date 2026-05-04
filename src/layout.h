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

/* Settings screen fixed input geometry */
#define SETTINGS_INPUT_W        350
#define SETTINGS_INPUT_Y_OFFSET 40

typedef struct {
  SDL_Point player_pos[MAX_PLAYERS]; /* top-left origin of each player's card row */
  SDL_Point timer;                   /* center of the circular countdown timer */
  SDL_Point table_center;            /* center of the table (pot coin landing point) */
  SDL_Rect  msg_panel;               /* status message panel rect */
  int       msg_panel_right;         /* x of right edge of msg_panel */
  int       action_btn_x;            /* left margin for action / amount buttons */
  int       status_line_h;           /* TTF line height of FONT_STATUS_MSG */

  struct {
    int title_x;              /* x of screen title widget (both screens) */
    int title_y;              /* y of screen title widget (both screens) */
    int margin_x;             /* left column x: connect inputs and settings x_left */
    int connect_btn_y;        /* y of Connect / Settings nav buttons */
    int connect_host_y;       /* y of host input field */
    int quit_y;               /* y of top-right X quit button */
    int settings_x_right;     /* right column x for settings grid */
    int settings_row_y[3];    /* y of each settings row (label baseline) */
    int settings_save_y;      /* y of Save / Load Defaults buttons */
    int back_img_x;           /* x of settings back-arrow image */
    int back_img_y;           /* y of settings back-arrow image */
    int links_center_x;       /* horizontal center for link widgets */
  } menu;

  struct {
    int waiting_y; /* y of "Waiting for players/dealer" status text */
    int kick_x;    /* x of Kick / Ban admin buttons */
    int kick_y;    /* y of Kick / Ban admin buttons */
  } lobby;
} GameLayout_t;

extern GameLayout_t g_layout;

/* Call once after fonts are open and g_viewport is set. */
void layout_init(int status_font_line_h);

/* Recompute all positions from g_viewport / g_center.
 * Called by layout_init and again by toggle_fullscreen. */
void layout_compute(void);

#endif
