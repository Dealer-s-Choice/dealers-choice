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

#define CARD_DEAL_DELAY 50

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

static int menu_display_game_choices(TCPsocket client_socket, SDLNet_SocketSet socket_set,
                                     const int8_t my_id, struct game_state_t *game_state,
                                     SDL_Renderer *renderer, struct font_t *font) {
  struct button_t button_5_card_draw = {
      .text = "5-card draw",
      .renderer = renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = {100, 160, 200, 40},
      .font = font->fonts[OTHER],
      .enabled = true,
  };

  bool running = true;
  while (running && game_state->at_menu) {
    recv_game_state(client_socket, socket_set, game_state);
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      int mx = e.button.x;
      int my = e.button.y;
      button_5_card_draw.enabled = (game_state->dealer_id == my_id);
      button_5_card_draw.hovered = SDL_PointInRect(&(SDL_Point){mx, my}, &button_5_card_draw.rect);
      if (e.type == SDL_QUIT) {
        return 1;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (point_in_rect(mx, my, &button_5_card_draw.rect) && game_state->dealer_id == my_id) {
          uint8_t msg = 0x01; // GAME_START
          SDLNet_TCP_Send(client_socket, &msg, sizeof(msg));
          running = false;
        }
      }
    }

    // Clear screen
    clear_screen(renderer);

    render_button(&button_5_card_draw);

    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (game_state->player[i].id != -1) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect input_box = make_rect(100, 250 + (i * 40), 200, 40);
        SDL_RenderDrawRect(renderer, &input_box);
        SDL_Rect input_text_pos = {input_box.x, input_box.y, 0, 0};
        render_text(renderer, font->fonts[OTHER], game_state->player[i].name,
                    get_color(COLOR_WHITE), &input_text_pos);
      }
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  return 0;
}

static void draw_card_back_pattern(SDL_Renderer *renderer, SDL_Rect *card_rect) {
  // Fill card with base color
  SDL_SetRenderDrawColor(renderer, 0, 0, 128, 255); // Dark blue
  SDL_RenderFillRect(renderer, card_rect);

  // Draw border
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White border
  SDL_RenderDrawRect(renderer, card_rect);

  // Draw pattern (e.g., diagonal crosshatch lines)
  SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255); // Light blue

  int spacing = 8;
  for (int y = 0; y < card_rect->h; y += spacing) {
    for (int x = 0; x < card_rect->w; x += spacing) {
      SDL_RenderDrawLine(renderer, card_rect->x + x, card_rect->y, card_rect->x, card_rect->y + y);
    }
  }

  for (int y = 0; y < card_rect->h; y += spacing) {
    for (int x = 0; x < card_rect->w; x += spacing) {
      SDL_RenderDrawLine(renderer, card_rect->x + x, card_rect->y + card_rect->h,
                         card_rect->x + card_rect->w, card_rect->y + y);
    }
  }
}

void run_sdl_loop(struct game_state_t *game_state, struct sdl_context_t *sdl_context,
                  struct font_t *font, TCPsocket client_socket, SDLNet_SocketSet socket_set,
                  const int8_t my_id) {

  const struct pos_t player_pos[MAX_PLAYERS] = {
      // P0: bottom center
      {.x = sdl_context->window_width / 3, .y = sdl_context->window_height - 80},

      // P1: left, 1/3 down
      {.x = 20, .y = sdl_context->window_height / 3},

      // P2: top-left
      {.x = 20, .y = 20},

      // P3: top-right
      {.x = sdl_context->window_width / 2 + 20, .y = 35},

      // P4: right, 1/3 down
      {.x = sdl_context->window_width / 2 + 20, .y = sdl_context->window_height / 3 + 15},
  };

  // This offers only a little extra protection if changes are made.
  _Static_assert(sizeof(player_pos) / sizeof(player_pos[0]) == 5,
                 "player_pos has wrong number of elements");

  // if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
  // fprintf(stderr, "SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
  //// handle error
  //}

  // Mix_Chunk *card_sound = Mix_LoadWAV("../card_dealt_stereo.wav");
  // if (!card_sound) {
  // fprintf(stderr, "Failed to load card sound! SDL_mixer Error: %s\n", Mix_GetError());
  //// handle error
  //}
  // Mix_VolumeChunk(card_sound, MIX_MAX_VOLUME / 2);

  // if (Mix_Paused(-1)) {
  // Mix_Resume(-1);
  //}
  enum {
    BET,
    PASS,
    FOLD,
    RAISE,
    CALL,
    ACTIONS_NUM,
  };

  const char *action[] = {
      [BET] = "Bet", [PASS] = "Pass", [FOLD] = "Fold", [RAISE] = "Raise", [CALL] = "Call",
  };

  int x_offset = 100;
  struct button_t action_button[ACTIONS_NUM];
  for (int i = 0; i < ACTIONS_NUM; i++) {
    struct pos_t butt_pos = {x_offset += 130, sdl_context->win_center.y + 20};
    action_button[i] =
        create_button(action[i], sdl_context->renderer, &butt_pos, font->fonts[OTHER]);
  }

  int running = 1;
  bool cards_dealt = false;
  while (running) {
    recv_game_state(client_socket, socket_set, game_state);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      int mx = event.button.x;
      int my = event.button.y;
      for (int i = 0; i < ACTIONS_NUM; i++) {
        action_button[i].enabled = false;
        action_button[i].hovered = SDL_PointInRect(&(SDL_Point){mx, my}, &action_button[i].rect);
      }

      if (event.type == SDL_QUIT) {
        running = 0;
      }
    }
    clear_screen(sdl_context->renderer);

    if (game_state->at_menu) {
      if (menu_display_game_choices(client_socket, socket_set, my_id, game_state,
                                    sdl_context->renderer, font) != 0)
        running = false;
      else {
        continue;
      }
    } else {
      for (int i = 0; i < HAND_SIZE; ++i) {
        for (int player_n = 0; player_n < MAX_PLAYERS; player_n++) {
          if (game_state->player[player_n].id == -1)
            continue;
          // Show each card that has been dealt

          int card_x = player_pos[player_n].x + i * (80 + 10);
          int card_y = player_pos[player_n].y;

          // Draw white card box
          SDL_Rect card_rect = {card_x, card_y, 80, 50};
          SDL_SetRenderDrawColor(sdl_context->renderer, 255, 255, 255, 255);
          SDL_RenderFillRect(sdl_context->renderer, &card_rect);
          SDL_SetRenderDrawColor(sdl_context->renderer, 0, 0, 0, 255);
          SDL_RenderDrawRect(sdl_context->renderer, &card_rect);

          // Render face + suit
          SDL_Color textColor;
          char text[8] = {0};
          if (game_state->player[player_n].hand.card[i].face_val != dh_card_back.face_val) {
            const char *face =
                get_card_face_str(game_state->player[player_n].hand.card[i].face_val);
            const char *suit = get_card_unicode_suit(game_state->player[player_n].hand.card[i]);
            snprintf(text, sizeof(text), "%s%s", face, suit);

            if (game_state->player[player_n].hand.card[i].suit == HEARTS ||
                game_state->player[player_n].hand.card[i].suit == DIAMONDS) {
              textColor = (SDL_Color){255, 0, 0, 255}; // Red
            } else {
              textColor = (SDL_Color){0, 0, 0, 255}; // Black
            }
          } else {
            draw_card_back_pattern(sdl_context->renderer, &card_rect);

            // SDL_RenderPresent(sdl_context->renderer); // Present *before* playing sound
            // Mix_PlayChannel(-1, card_sound, 0);
            if (!cards_dealt) {
              Uint32 start = SDL_GetTicks();
              while (SDL_GetTicks() - start < CARD_DEAL_DELAY) {
                SDL_Event e;
                while (SDL_PollEvent(&e)) {
                  if (e.type == SDL_QUIT) {
                    running = 0;
                    break;
                  }
                }
              }
              SDL_RenderPresent(sdl_context->renderer);
              SDL_Delay(16); // Let audio play & system breathe
            }

            continue;
          }

          SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font->fonts[CARD], text, textColor);
          SDL_Texture *textTexture =
              SDL_CreateTextureFromSurface(sdl_context->renderer, textSurface);

          SDL_Rect textRect = {card_x + (80 - textSurface->w) / 2,
                               card_y + (50 - textSurface->h) / 2, textSurface->w, textSurface->h};

          SDL_RenderCopy(sdl_context->renderer, textTexture, NULL, &textRect);
          SDL_FreeSurface(textSurface);
          SDL_DestroyTexture(textTexture);

          // Mix_PlayChannel(-1, card_sound, 0); // -1 = first available channel, 0 = play once
          // SDL_Delay(500);
          if (!cards_dealt) {
            Uint32 start = SDL_GetTicks();
            while (SDL_GetTicks() - start < CARD_DEAL_DELAY) {
              SDL_Event e;
              while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT)
                  running = false;
              }
            }
            SDL_RenderPresent(sdl_context->renderer);
            SDL_Delay(16);
          }
        }
      }

      for (int i = 0; i < ACTIONS_NUM; i++)
        render_button(&action_button[i]);

      cards_dealt = true;
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "pot: %d", game_state->pot);
      SDL_Color black = {0, 0, 0, 255};
      render_text_centered(sdl_context->renderer, font->fonts[OTHER], buffer, black,
                           sdl_context->win_center);

      for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game_state->player[i].id == -1)
          continue;

        draw_silver_coin(sdl_context->renderer, player_pos[i].x, player_pos[i].y - 50);
        char coins_text[24] = {0};
        snprintf(coins_text, sizeof coins_text, " = %d", game_state->player[i].chips);
        SDL_Rect dest = {player_pos[i].x + 30, player_pos[i].y - 50, 40, 20};
        render_text_plain(sdl_context->renderer, font->fonts[OTHER], coins_text,
                          get_color(COLOR_BLACK), &dest);
      }
    }

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }
  // Mix_FreeChunk(card_sound);
  // Mix_CloseAudio();
}
