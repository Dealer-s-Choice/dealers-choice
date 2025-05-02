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

#include "graphics.h"

static const SDL_Color color_table[COLOR_COUNT] = {
    [COLOR_WHITE] = {255, 255, 255, 255}, [COLOR_LIGHTGRAY] = {200, 200, 200, 255},
    [COLOR_GRAY] = {128, 128, 128, 255},  [COLOR_DARKGRAY] = {64, 64, 64, 255},
    [COLOR_BLACK] = {0, 0, 0, 255},       [COLOR_RED] = {255, 0, 0, 255},
    [COLOR_GREEN] = {0, 255, 0, 255},     [COLOR_GREEN_ONE] = {40, 120, 70, 255},
    [COLOR_BLUE] = {0, 0, 255, 255},      [COLOR_YELLOW] = {255, 255, 0, 255},
    [COLOR_CYAN] = {0, 255, 255, 255},    [COLOR_MAGENTA] = {255, 0, 255, 255},
    [COLOR_ORANGE] = {255, 165, 0, 255},  [COLOR_PURPLE] = {128, 0, 128, 255},
    [COLOR_BROWN] = {165, 42, 42, 255},   [COLOR_PINK] = {255, 192, 203, 255},
    [COLOR_TEAL] = {0, 128, 128, 255},
};

static const char *color_names[COLOR_COUNT] = {
    "white",  "lightgray", "gray",    "darkgray", "black",  "red",   "green", "green_one", "blue",
    "yellow", "cyan",      "magenta", "orange",   "purple", "brown", "pink",  "teal"};

SDL_Color get_color(ColorName name) {
  if (name < 0 || name >= COLOR_COUNT)
    return (SDL_Color){0, 0, 0, 255}; // fallback
  return color_table[name];
}

const char *get_color_name(ColorName name) {
  if (name < 0 || name >= COLOR_COUNT)
    return "unknown";
  return color_names[name];
}

const struct font_args_t font_args[] = {
    [CARD] = {.file = "../src/LiberationMono-Regular.ttf", .ptsize = 38},
    [OTHER] = {.file = "../src/LiberationSerif-Bold.ttf", .ptsize = 30},
};

void init_sdl_window(struct sdl_context_t *sdl_context, const char *title, int w, int h) {
  SDL_Init(SDL_INIT_VIDEO);
  sdl_context->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w,
                                         h, SDL_WINDOW_SHOWN);
  if (!sdl_context->window)
    puts(SDL_GetError());
  sdl_context->renderer = SDL_CreateRenderer(sdl_context->window, -1, SDL_RENDERER_ACCELERATED);
  if (!sdl_context->renderer)
    puts(SDL_GetError());
  return;
}

static struct pos_t get_window_center_pos(SDL_Window *window) {
  struct pos_t pos, w_center_pos;
  SDL_GetWindowSize(window, &pos.x, &pos.y);
  w_center_pos.x = pos.x / 2;
  w_center_pos.y = pos.y / 2;
  return w_center_pos;
}

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

static void recv_game_state(TCPsocket client_socket, SDLNet_SocketSet socket_set,
                            struct game_state_t *game_state) {
  if (SDLNet_CheckSockets(socket_set, 0) > 0 && SDLNet_SocketReady(client_socket)) {
    uint32_t size_net = 0;
    if (recv_all_tcp(client_socket, &size_net, sizeof(size_net)) == 0) {
      uint32_t size = ntohl(size_net);
      uint8_t *buffer = malloc(size);
      if (buffer) {
        if (recv_all_tcp(client_socket, buffer, size) == 0)
          *game_state = deserialize_game_state(buffer, size);

        free(buffer);
      }
    }
  }
}

SDL_Rect make_rect(int x, int y, int w, int h) {
  SDL_Rect r = {x, y, w, h};
  return r;
}

bool point_in_rect(int x, int y, SDL_Rect *r) {
  return x >= r->x && x <= (r->x + r->w) && y >= r->y && y <= (r->y + r->h);
}

void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                 SDL_Rect *dest) {
  if (!text || strlen(text) == 0) {
    fprintf(stderr, "Warning: Empty or null text passed to render_text.\n");
    return;
  }
  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
  if (!surface) {
    fprintf(stderr, "TTF_RenderUTF8_Blended error: %s\n", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  dest->w = surface->w;
  dest->h = surface->h;

  SDL_RenderCopy(renderer, texture, NULL, dest);

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

void make_button(struct button_t *button) {
  // Draw the filled background
  SDL_SetRenderDrawColor(button->renderer, button->bg_color.r, button->bg_color.g,
                         button->bg_color.b, button->bg_color.a);
  SDL_RenderFillRect(button->renderer, &button->rect);

  // 3D border effect
  SDL_SetRenderDrawColor(button->renderer, 255, 255, 255, 255); // Top-left (light)
  SDL_RenderDrawLine(button->renderer, button->rect.x, button->rect.y,
                     button->rect.x + button->rect.w - 1, button->rect.y); // Top
  SDL_RenderDrawLine(button->renderer, button->rect.x, button->rect.y, button->rect.x,
                     button->rect.y + button->rect.h - 1); // Left

  SDL_SetRenderDrawColor(button->renderer, 64, 64, 64, 255); // Bottom-right (dark)
  SDL_RenderDrawLine(button->renderer, button->rect.x, button->rect.y + button->rect.h - 1,
                     button->rect.x + button->rect.w - 1,
                     button->rect.y + button->rect.h - 1); // Bottom
  SDL_RenderDrawLine(button->renderer, button->rect.x + button->rect.w - 1, button->rect.y,
                     button->rect.x + button->rect.w - 1,
                     button->rect.y + button->rect.h - 1); // Right

  // Render the text centered on the button
  SDL_Surface *textSurface = TTF_RenderUTF8_Blended(button->font, button->text, button->fg_color);
  if (!textSurface)
    return;

  SDL_Texture *textTexture = SDL_CreateTextureFromSurface(button->renderer, textSurface);

  int text_x = button->rect.x + (button->rect.w - textSurface->w) / 2;
  int text_y = button->rect.y + (button->rect.h - textSurface->h) / 2;
  SDL_Rect textRect = {text_x, text_y, textSurface->w, textSurface->h};

  SDL_RenderCopy(button->renderer, textTexture, NULL, &textRect);

  SDL_FreeSurface(textSurface);
  SDL_DestroyTexture(textTexture);
}

void run_sdl_loop(struct game_state_t *game_state, TCPsocket client_socket,
                  SDLNet_SocketSet socket_set) {
  struct sdl_context_t sdl_context;
  init_sdl_window(&sdl_context, "Dealer's Choice", WINDOW_WIDTH, WINDOW_HEIGHT);

  struct font_t font;
  for (int i = 0; i < NUM_FONTS; ++i) {
    font.fonts[i] = open_font(&font_args[i]);
    if (!font.fonts[i])
      return; // or handle error
  }

  struct pos_t w_center_pos = get_window_center_pos(sdl_context.window);

  int running = 1;
  while (running) {
    recv_game_state(client_socket, socket_set, game_state);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = 0;
      }
    }

    // Background: dark green (poker table color)
    SDL_SetRenderDrawColor(sdl_context.renderer, 0, 125, 0, 255);
    SDL_RenderClear(sdl_context.renderer);

    if (!game_state->at_menu) {
    } else {

      for (int player_n = 0; player_n < MAX_PLAYERS; player_n++) {
        if (game_state->player[player_n].id == -1)
          continue;
        // Draw each card
        for (int i = 0; i < HAND_SIZE; ++i) {
          int card_x = game_state->player[player_n].pos.x + i * (80 + 10);
          int card_y = game_state->player[player_n].pos.y;

          // Draw white card box
          SDL_Rect card_rect = {card_x, card_y, 80, 50};
          SDL_SetRenderDrawColor(sdl_context.renderer, 255, 255, 255, 255);
          SDL_RenderFillRect(sdl_context.renderer, &card_rect);
          SDL_SetRenderDrawColor(sdl_context.renderer, 0, 0, 0, 255);
          SDL_RenderDrawRect(sdl_context.renderer, &card_rect);

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
          SDL_Texture *textTexture =
              SDL_CreateTextureFromSurface(sdl_context.renderer, textSurface);

          SDL_Rect textRect = {card_x + (80 - textSurface->w) / 2,
                               card_y + (50 - textSurface->h) / 2, textSurface->w, textSurface->h};

          SDL_RenderCopy(sdl_context.renderer, textTexture, NULL, &textRect);
          SDL_FreeSurface(textSurface);
          SDL_DestroyTexture(textTexture);
        }
      }

      char buffer[128];
      snprintf(buffer, sizeof(buffer), "pot: %d", game_state->pot);
      SDL_Color black = {0, 0, 0, 255};
      render_text_centered(sdl_context.renderer, font.fonts[OTHER], buffer, black, w_center_pos);
    }

    SDL_RenderPresent(sdl_context.renderer);
    SDL_Delay(16);
  }

  for (int i = 0; i < NUM_FONTS; ++i)
    TTF_CloseFont(font.fonts[i]);
}

void do_sdl_cleanup(struct sdl_context_t *sdl_context) {
  SDL_DestroyRenderer(sdl_context->renderer);
  SDL_DestroyWindow(sdl_context->window);
}
