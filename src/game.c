/*
 net.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025 Andy Alt

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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "game.h"

int menu_display_game(struct game_state_t *game_state, SDL_Renderer *renderer,
                      struct font_t *font) {
  struct button_t button_5_card_draw = {
      .text = "5-card draw",
      .renderer = renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = {100, 160, 200, 40},
      .pos = {100, 160},
      .font = font->fonts[OTHER],
  };

  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      int mx = e.button.x;
      int my = e.button.y;
      button_5_card_draw.hovered = SDL_PointInRect(&(SDL_Point){mx, my}, &button_5_card_draw.rect);
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (point_in_rect(mx, my, &button_5_card_draw.rect)) {
          game_state->at_menu = false;
          running = false;
        }
      }
    }

    // Clear screen
    clear_screen(renderer);

    make_button(&button_5_card_draw);

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  return 0;
}
