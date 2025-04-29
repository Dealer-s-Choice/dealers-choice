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

  return;
}

static struct pos_t get_window_center_pos(SDL_Window *window) {
  struct pos_t pos, w_center_pos;
  SDL_GetWindowSize(window, &pos.x, &pos.y);
  w_center_pos.x = pos.x / 2;
  w_center_pos.y = pos.y / 2;
  return w_center_pos;
}

struct font_args_t {
  const char *file;
  const int ptsize;
};

enum { CARD, OTHER, NUM_FONTS };

const struct font_args_t font_args[NUM_FONTS] = {
    [CARD] = {.file = "../src/LiberationMono-Regular.ttf", .ptsize = 38},
    [OTHER] = {.file = "../src/LiberationSerif-Bold.ttf", .ptsize = 30},
};

struct font_t {
  TTF_Font *fonts[NUM_FONTS];
};

TTF_Font *open_font(const struct font_args_t *args) {
  TTF_Font *font = TTF_OpenFont(args->file, args->ptsize);
  if (!font)
    fprintf(stderr, "Failed to load font (%s): %s\n", args->file, TTF_GetError());
  return font;
}

void render_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                          struct pos_t center) {
  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
  if (!surface) {
    fprintf(stderr, "TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  int w, h;
  SDL_QueryTexture(texture, NULL, NULL, &w, &h);

  SDL_Rect dst = {.x = center.x - w / 2, .y = center.y - h / 2, .w = w, .h = h};

  SDL_RenderCopy(renderer, texture, NULL, &dst);

  SDL_DestroyTexture(texture);
  SDL_FreeSurface(surface);
}

void run_sdl_loop(struct sdl_context_t *sdl_context, struct game_state_t *game_state) {
  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return;
  }

  struct font_t font;
  for (int i = 0; i < NUM_FONTS; ++i) {
    font.fonts[i] = open_font(&font_args[i]);
    if (!font.fonts[i])
      return; // or handle error
  }

  struct pos_t w_center_pos = get_window_center_pos(sdl_context->window);

  int running = 1;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = 0;
      }
    }

    // Background: dark green (poker table color)
    SDL_SetRenderDrawColor(sdl_context->renderer, 0, 125, 0, 255);
    SDL_RenderClear(sdl_context->renderer);

    for (int player_n = 0; player_n < MAX_PLAYERS; player_n++) {
      if (game_state->player[player_n].id == -1)
        continue;
      // Draw each card
      for (int i = 0; i < HAND_SIZE; ++i) {
        int card_x = game_state->player[player_n].pos.x + i * (80 + 10);
        int card_y = game_state->player[player_n].pos.y;

        // Draw white card box
        SDL_Rect card_rect = {card_x, card_y, 80, 50};
        SDL_SetRenderDrawColor(sdl_context->renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(sdl_context->renderer, &card_rect);
        SDL_SetRenderDrawColor(sdl_context->renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(sdl_context->renderer, &card_rect);

        // Render face + suit
        const char *face = get_card_face_str(game_state->player[player_n].hand.card[i].face_val);
        const char *suit = get_card_unicode_suit(game_state->player[player_n].hand.card[i]);

        char text[8];
        snprintf(text, sizeof(text), "%s%s", face, suit);

        SDL_Color textColor;
        if (game_state->player[player_n].hand.card[i].suit == HEARTS ||
            game_state->player[player_n].hand.card[i].suit == DIAMONDS) {
          textColor = (SDL_Color){255, 0, 0, 255}; // Red
        } else {
          textColor = (SDL_Color){0, 0, 0, 255}; // Black
        }

        SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font.fonts[CARD], text, textColor);
        SDL_Texture *textTexture = SDL_CreateTextureFromSurface(sdl_context->renderer, textSurface);

        SDL_Rect textRect = {card_x + (80 - textSurface->w) / 2, card_y + (50 - textSurface->h) / 2,
                             textSurface->w, textSurface->h};

        SDL_RenderCopy(sdl_context->renderer, textTexture, NULL, &textRect);
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
      }
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "pot: %d", game_state->pot);
    SDL_Color black = {0, 0, 0, 255};
    render_text_centered(sdl_context->renderer, font.fonts[OTHER], buffer, black, w_center_pos);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  for (int i = 0; i < NUM_FONTS; ++i)
    TTF_CloseFont(font.fonts[i]);

  TTF_Quit();
}

void do_sdl_cleanup(struct sdl_context_t *sdl_context) {
  SDL_DestroyRenderer(sdl_context->renderer);
  SDL_DestroyWindow(sdl_context->window);
  SDL_Quit();
}
