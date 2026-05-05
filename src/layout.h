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
  int button_w_pad;
  int button_h_pad;
  int input_text_pad_x;
  int input_h_pad;
} LayoutConfig_t;

extern LayoutConfig_t g_layout_cfg;

LayoutConfig_t get_layout_config(const char *data_dir);

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
