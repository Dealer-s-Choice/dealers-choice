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

#include "types.h"
#include <SDL2/SDL.h>

/* Number of scrolling status message lines shown in the panel.
 * Must remain a compile-time constant (used as static array dimension). */
#define SIZEOF_STATUS_MSGS 16

typedef struct {
  int margin;
  int button_x_spacing;
  int back_btn_size;
  int circle_timer_r;
  int msg_panel_x_offset;
  int msg_panel_w;
  int msg_panel_pad_x;
  int msg_panel_pad_y;
  int settings_input_w;
  int settings_input_y_offset;
  int card_w;
  int card_h;
  int card_padding;
  int link_pad_x;
  int link_pad_y;
  int action_btn_x_gap;
  int menu_title_y;
  int menu_margin_x_offset;
  int menu_connect_btn_y_offset;
  int menu_connect_host_y_offset;
  int menu_settings_x_right_offset;
  int menu_settings_x_third_offset;
  int menu_settings_row_y_0;
  int menu_settings_row_y_1;
  int menu_settings_row_y_2;
  int menu_settings_save_y_offset;
  int menu_links_center_x_offset;
  int lobby_waiting_from_bottom;
  int lobby_kick_x_divisor;
  int lobby_kick_y_pct;
  int kick_ban_btn_gap;
  int game_kick_y_gap;
  int pot_boundary;
  int board_y_offset;
  int timer_border;
  int indicator_pad;
  int indicator_min_r;
  int nameplate_pad;
  int open_card_pad;
  int nameplate_radius;
  int confirm_quit_pad;
  int confirm_quit_btn_gap;
  int connect_settings_btn_gap;
  int input_field_v_gap;
  int connect_input_w_pad;
  int connect_save_btn_gap;
  int settings_save_btn_gap;
  int version_x_offset;
  int version_y_offset;
  int checkbox_pad;
  int input_text_pad_x;
  int input_h_pad;
  int timer_status_gap;      /* px between status panel top and timer bottom */
  int community_top_offset;  /* y of community card row from viewport top */
  int discard_overlay_alpha; /* alpha of the discard-hint overlay over the hand */
  int btn_hand_gap;          /* px between action button row and local hand top */
  int dash_pad;              /* dashboard inner padding */
  int dash_divider;          /* dark-green 3D divider between dashboard cells */
  int dash_row_div;          /* dark-green 3D border between amount and action rows */
  int dash_btn_div;          /* thin 3D border between adjacent buttons in a row */
  int amount_btn_min_w;      /* readable floor for shrunk amount buttons */
  int slider_w;              /* bet-amount slider track width */
  int indicator_cell_pad;    /* padding around each indicator inside its cell (the "T" top) */
  int dash_x_offset;         /* shift the whole dashboard right of window center */
  int act_btn_gap;           /* horizontal gap between adjacent action buttons */
} LayoutConfig_t;

extern LayoutConfig_t g_layout_cfg;

LayoutConfig_t get_layout_config(const char *data_dir);

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
