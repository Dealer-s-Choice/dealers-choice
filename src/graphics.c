/*
 graphics.c
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

#include <SDL2/SDL_ttf.h>

#include "graphics.h"

void init_sdl_window(struct sdl_context_t *sdl_context, const char *title) {
  SDL_Init(SDL_INIT_VIDEO);
  const char *client = strstr(title, "Client");
  int win_pos_x = (client != NULL) ? WINDOW_WIDTH / 2 + 10 : SDL_WINDOWPOS_CENTERED;
  int win_pos_y = (client != NULL) ? WINDOW_HEIGHT / 2 + 10 : SDL_WINDOWPOS_CENTERED;
  sdl_context->window =
      SDL_CreateWindow(title, win_pos_x, win_pos_y, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
  sdl_context->renderer = SDL_CreateRenderer(sdl_context->window, -1, SDL_RENDERER_ACCELERATED);

  //// Draw white background
  // SDL_SetRenderDrawColor(sdl_context->renderer, 0, 125, 0, 255);
  // SDL_RenderClear(sdl_context->renderer);
  // SDL_RenderPresent(sdl_context->renderer);
  return;
}

void run_sdl_loop(SDL_Renderer *renderer, struct player_t *player) {
  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return;
  }

  TTF_Font *font =
      TTF_OpenFont("../src/LiberationMono-Regular.ttf", 38); // make sure this font file exists
  if (!font) {
    fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
    return;
  }

  int running = 1;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = 0;
      }
    }

    // Background: dark green (poker table color)
    SDL_SetRenderDrawColor(renderer, 0, 125, 0, 255);
    SDL_RenderClear(renderer);

    // Draw each card
    for (int i = 0; i < HAND_SIZE; ++i) {
      int card_x = player->pos.x + i * (80 + 10);
      int card_y = player->pos.y;

      // Draw white card box
      SDL_Rect card_rect = {card_x, card_y, 80, 50};
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_RenderFillRect(renderer, &card_rect);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderDrawRect(renderer, &card_rect);

      // Render face + suit
      const char *face = get_card_face_str(player->hand.card[i].face_val);
      const char *suit = get_card_unicode_suit(player->hand.card[i]);

      char text[8];
      snprintf(text, sizeof(text), "%s%s", face, suit);

      SDL_Color textColor;
      if (player->hand.card[i].suit == HEARTS || player->hand.card[i].suit == DIAMONDS) {
        textColor = (SDL_Color){255, 0, 0, 255}; // Red
      } else {
        textColor = (SDL_Color){0, 0, 0, 255}; // Black
      }

      SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, text, textColor);
      SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

      SDL_Rect textRect = {card_x + (80 - textSurface->w) / 2, card_y + (50 - textSurface->h) / 2,
                           textSurface->w, textSurface->h};

      SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
      SDL_FreeSurface(textSurface);
      SDL_DestroyTexture(textTexture);
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  TTF_CloseFont(font);
  TTF_Quit();
}

void do_sdl_cleanup(struct sdl_context_t *sdl_context) {
  SDL_DestroyRenderer(sdl_context->renderer);
  SDL_DestroyWindow(sdl_context->window);
  SDL_Quit();
}
