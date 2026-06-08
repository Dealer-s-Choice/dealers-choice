/*
 layout.c
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

#include "layout.h"
#include "globals.h"

GameLayout_t g_layout;
LayoutConfig_t g_layout_cfg;

void layout_init(int status_font_line_h) {
  g_layout.status_line_h = status_font_line_h;
  layout_compute();
}

void layout_compute(void) {
  SDL_Rect vp = g_viewport;
  const LayoutConfig_t *c = &g_layout_cfg;

  int right_x = vp.x + vp.w - (c->card_w * 7 + c->card_padding * 7 + c->margin);

  g_layout.player_pos[0].x = vp.x + c->margin;
  g_layout.player_pos[0].y = vp.y + c->card_h * 4;

  g_layout.player_pos[1].x = vp.x + c->margin;
  g_layout.player_pos[1].y = vp.y + c->card_h;

  g_layout.player_pos[2].x = right_x;
  g_layout.player_pos[2].y = vp.y + c->card_h;

  g_layout.player_pos[3].x = right_x;
  g_layout.player_pos[3].y = vp.y + c->card_h * 4;

  g_layout.player_pos[4].x = right_x;
  g_layout.player_pos[4].y = vp.y + c->card_h * 7;

  /* Local player's hand sits bottom-center. x is the row CENTER (layout_seats_for
   * subtracts half the row width); y is the row top, lifted to leave room for the
   * local nameplate drawn just below the cards. */
  g_layout.local_seat.x = g_center.x + c->dash_x_offset;
  g_layout.local_seat.y = vp.y + vp.h - c->card_h * 3;

  g_layout.table_center.x = g_center.x;
  {
    /* Pot sits halfway between the community-card row (top) and the dashboard
     * (bottom). Its scatter radius is bounded so coins clear the community cards,
     * the dashboard, the message window, and the right seat column. */
    int community_bottom = vp.y + c->community_top_offset + c->card_h;
    int dash_h = c->circle_timer_r * 2 + 2 * c->dash_pad;
    int dash_top = g_layout.local_seat.y - c->btn_hand_gap - dash_h;
    g_layout.table_center.y = (community_bottom + dash_top) / 2;

    int up = g_layout.table_center.y - community_bottom;
    int left = g_layout.table_center.x - (vp.x + c->msg_panel_x_offset + c->msg_panel_w);
    int right = right_x - g_layout.table_center.x;
    int rad = up;
    if (left < rad)
      rad = left;
    if (right < rad)
      rad = right;
    g_layout.pot_radius = rad > 0 ? rad : 0;
  }

  g_layout.msg_panel.x = vp.x + c->msg_panel_x_offset;
  g_layout.msg_panel.y = g_center.y;
  g_layout.msg_panel.w = c->msg_panel_w;
  g_layout.msg_panel.h =
      g_layout.status_line_h > 0
          ? (g_layout.status_line_h + 2) * SIZEOF_STATUS_MSGS + c->msg_panel_pad_y * 2
          : 0;

  g_layout.msg_panel_right = g_layout.msg_panel.x + g_layout.msg_panel.w;

  g_layout.action_btn_x = g_layout.msg_panel_right + c->action_btn_x_gap;

  /* Countdown timer sits directly above the status panel, centered on it, with
   * timer_status_gap px between the timer's bottom edge and the panel's top. */
  g_layout.timer.x = g_layout.msg_panel.x + g_layout.msg_panel.w / 2;
  g_layout.timer.y = g_layout.msg_panel.y - c->timer_status_gap - c->circle_timer_r;

  /* Menu screens (connect + settings) */
  g_layout.menu.title_x = g_center.x * 2 / 3;
  g_layout.menu.title_y = c->menu_title_y;
  g_layout.menu.margin_x = vp.x + c->menu_margin_x_offset;
  g_layout.menu.connect_btn_y = vp.y + c->menu_connect_btn_y_offset;
  g_layout.menu.connect_host_y = vp.y + c->menu_connect_host_y_offset;
  g_layout.menu.quit_y = vp.y + c->margin;
  g_layout.menu.settings_x_right = vp.x + c->menu_settings_x_right_offset;
  g_layout.menu.settings_x_third = vp.x + c->menu_settings_x_third_offset;
  g_layout.menu.settings_row_y[0] = vp.y + c->menu_settings_row_y_0;
  g_layout.menu.settings_row_y[1] = vp.y + c->menu_settings_row_y_1;
  g_layout.menu.settings_row_y[2] = vp.y + c->menu_settings_row_y_2;
  g_layout.menu.settings_save_y = vp.y + c->menu_settings_save_y_offset;
  g_layout.menu.back_img_x = vp.x + vp.w - c->back_btn_size - c->margin;
  g_layout.menu.back_img_y = vp.y + vp.h / 2;
  g_layout.menu.links_center_x = g_center.x + c->menu_links_center_x_offset;

  /* Lobby (game selection) screen */
  g_layout.lobby.waiting_y = vp.y + vp.h - c->lobby_waiting_from_bottom;
  g_layout.lobby.kick_x = vp.x + vp.w / c->lobby_kick_x_divisor;
  g_layout.lobby.kick_y = vp.y + vp.h * c->lobby_kick_y_pct / 100;
}

SDL_Rect rect_anchored(SDL_Point ref, int w, int h, Anchor_t a, int dx, int dy) {
  int col = a % 3; /* 0 left, 1 center, 2 right */
  int row = a / 3; /* 0 top, 1 mid, 2 bottom */
  SDL_Rect r = {ref.x - col * w / 2 + dx, ref.y - row * h / 2 + dy, w, h};
  return r;
}

void layout_seats_for(SDL_Point out[MAX_PLAYERS], int local_id, int local_row_w) {
  if (local_id < 0 || local_id >= MAX_PLAYERS) {
    for (int g = 0; g < MAX_PLAYERS; g++)
      out[g] = g_layout.player_pos[g];
    return;
  }
  for (int g = 0; g < MAX_PLAYERS; g++) {
    if (g == local_id) {
      out[g].x = g_layout.local_seat.x - local_row_w / 2;
      out[g].y = g_layout.local_seat.y;
    } else {
      int d = (g - local_id + MAX_PLAYERS) % MAX_PLAYERS; /* 1..MAX_PLAYERS-1 */
      out[g] = g_layout.player_pos[d - 1];
    }
  }
}
