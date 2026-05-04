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

#include "globals.h"      /* full dep chain: tcpme → types.h; also g_viewport, g_center */
#include "layout.h"       /* GameLayout_t, layout_init, layout_compute */
#include "graphics.h"     /* MARGIN */
#include "widgets/card.h" /* CARD_W, CARD_H, CARD_PADDING */

GameLayout_t g_layout;

void layout_init(int status_font_line_h) {
  g_layout.status_line_h = status_font_line_h;
  layout_compute();
}

void layout_compute(void) {
  SDL_Rect vp = g_viewport;

  int right_x = vp.x + vp.w - (CARD_W * 7 + CARD_PADDING * 7 + MARGIN);

  g_layout.player_pos[0].x = vp.x + MARGIN;
  g_layout.player_pos[0].y = vp.y + CARD_H * 4;

  g_layout.player_pos[1].x = vp.x + MARGIN;
  g_layout.player_pos[1].y = vp.y + CARD_H;

  g_layout.player_pos[2].x = right_x;
  g_layout.player_pos[2].y = vp.y + CARD_H;

  g_layout.player_pos[3].x = right_x;
  g_layout.player_pos[3].y = vp.y + CARD_H * 4;

  g_layout.player_pos[4].x = right_x;
  g_layout.player_pos[4].y = vp.y + CARD_H * 7;

  g_layout.timer.x = g_center.x;
  g_layout.timer.y = vp.y + vp.h - MARGIN - CIRCLE_TIMER_R;

  g_layout.table_center.x = g_center.x;
  g_layout.table_center.y = g_center.y;

  g_layout.msg_panel.x = vp.x + MSG_PANEL_X_OFFSET;
  g_layout.msg_panel.y = g_center.y;
  g_layout.msg_panel.w = MSG_PANEL_W;
  g_layout.msg_panel.h = g_layout.status_line_h > 0
      ? (g_layout.status_line_h + 2) * SIZEOF_STATUS_MSGS + MSG_PANEL_PAD_Y * 2
      : 0;

  g_layout.msg_panel_right = g_layout.msg_panel.x + g_layout.msg_panel.w;

  /* Buttons start just to the right of the message panel */
  g_layout.action_btn_x = g_layout.msg_panel_right + 50;
}
