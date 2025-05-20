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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "game.h"

// Build fails using gcc on Ubuntu 24.04 (and maybe others) without this
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CARD_DEAL_DELAY 50

void free_player_list(struct player_list_t *head) {
  if (!head)
    return;

  struct player_list_t *current = head->next;
  struct player_list_t *prev = head;

  while (current && current != head) {
    struct player_list_t *next = current->next;
    free(prev);
    prev = current;
    current = next;
  }

  free(prev); // Free the last node (head if only one node)
}

struct player_list_t *create_player_list(game_state_t *game_state) {
  struct player_list_t *root = NULL;
  struct player_list_t *tail = NULL;
  game_state->player_count = 0;

  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (game_state->player[i].in == false)
      continue;

    struct player_list_t *new_node = malloc(sizeof(*new_node));
    if (!new_node) {
      perror("malloc");
      fputs("Could not create player list\n", stderr);
      free_player_list(root);
      return NULL;
    }

    new_node->id = game_state->player[i].id;
    game_state->player_count++;
    new_node->next = NULL;

    if (!root) {
      root = new_node;
    } else {
      tail->next = new_node;
    }

    tail = new_node;
  }

  if (tail)
    tail->next = root; // Close the circle

  return root;
}

static int8_t send_game_select(TCPsocket sock, uint8_t game_type) {
  uint8_t buffer[3];
  buffer[0] = (MSG_GAME_SELECT >> 8) & 0xFF;
  buffer[1] = (MSG_GAME_SELECT) & 0xFF;
  buffer[2] = game_type;

  return send_all_tcp(sock, buffer, sizeof(buffer));
}

static int menu_display_game_choices(TCPsocket client_socket, SDLNet_SocketSet socket_set,
                                     const int8_t my_id, game_state_t *game_state,
                                     struct sdl_context_t *sdl_context, struct font_t *font) {

  // TODO: Now that we're adding more buttons, this will get refactored to prevent
  // duplication (and gobs and gobs of code).
  int y_offset = 160;
  int button_height = 40;
  struct button_t button_5_card_draw = {
      .text = "5-card draw",
      .renderer = sdl_context->renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = {100, y_offset, 200, button_height},
      .font = font->fonts[OTHER],
      .enabled = true,
  };

  y_offset += button_height * 1.1;
  struct button_t button_5_card_stud = {
      .text = "5-card stud",
      .renderer = sdl_context->renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = {100, y_offset, 200, button_height},
      .font = font->fonts[OTHER],
      .enabled = true,
  };

  bool running = true;
  while (running && game_state->at_menu) {
    if (recv_game_state(client_socket, socket_set, game_state) == -1)
      return -1;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      int mx = e.button.x;
      int my = e.button.y;
      button_5_card_draw.enabled = (game_state->dealer_id == my_id);
      button_5_card_stud.enabled = (game_state->dealer_id == my_id);
      button_5_card_draw.hovered = SDL_PointInRect(&(SDL_Point){mx, my}, &button_5_card_draw.rect);
      button_5_card_stud.hovered = SDL_PointInRect(&(SDL_Point){mx, my}, &button_5_card_stud.rect);
      if (e.type == SDL_QUIT) {
        return 1;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (point_in_rect(mx, my, &button_5_card_draw.rect) && game_state->dealer_id == my_id) {
          if (send_game_select(client_socket, GAME_5_CARD_DRAW) == 0)
            puts("Game type sent");
          else
            return -1;
          running = false;
        } else {
          if (point_in_rect(mx, my, &button_5_card_stud.rect) && game_state->dealer_id == my_id) {
            if (send_game_select(client_socket, GAME_5_CARD_STUD) == 0)
              puts("Game type sent");
            else
              return -1;
            running = false;
          }
        }
      }
    }

    // Clear screen
    clear_screen(sdl_context->renderer);

    render_button(&button_5_card_draw);
    render_button(&button_5_card_stud);

    SDL_Point status_pos = {
        sdl_context->window_width * .1,
        sdl_context->window_height / 2,
    };
    int offset_x = status_pos.x, offset_y = status_pos.y;

    SDL_Rect text_connected = {offset_x, offset_y, 0, 0};
    render_text_plain(sdl_context->renderer, font->fonts[OTHER],
                      "Connected players:", get_color(COLOR_BLACK), &text_connected);
    offset_x += 10;

    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (game_state->player[i].in) {
        offset_y += 40;
        char tmp[sizeof(game_state->player[i].name) + 20] = {0};
        snprintf(tmp, sizeof tmp, "%s%s", game_state->player[i].name,
                 game_state->dealer_id == i ? " (Dealer)" : "");
        SDL_Rect text_pos = {offset_x, offset_y, 0, 0};
        render_text_plain(sdl_context->renderer, font->fonts[OTHER], tmp, get_color(COLOR_WHITE),
                          &text_pos);
      }
    }

    SDL_RenderPresent(sdl_context->renderer);
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

static int8_t send_player_action(TCPsocket sock, uint8_t action, uint32_t amount) {
  uint8_t buffer[7];

  buffer[0] = (MSG_PLAYER_ACTION >> 8) & 0xFF;
  buffer[1] = (MSG_PLAYER_ACTION) & 0xFF;
  buffer[2] = action;

  buffer[3] = (amount >> 24) & 0xFF;
  buffer[4] = (amount >> 16) & 0xFF;
  buffer[5] = (amount >> 8) & 0xFF;
  buffer[6] = (amount) & 0xFF;

  return send_all_tcp(sock, buffer, sizeof(buffer));
}

static bool is_dh_card_back(struct dh_card a) {
  return a.face_val == dh_card_back.face_val && a.suit == dh_card_back.suit;
}

static bool is_dh_card_null(struct dh_card a) {
  return a.face_val == dh_card_null.face_val && a.suit == dh_card_null.suit;
}

static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius,
                               SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int y = -radius; y <= radius; y++) {
    int dx = (int)sqrt(radius * radius - y * y);
    SDL_RenderDrawLine(renderer, cx - dx, cy + y, cx + dx, cy + y);
  }
}

static void draw_circle_outline(SDL_Renderer *renderer, int cx, int cy, int radius,
                                SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  const int points = 100;
  for (int i = 0; i < points; ++i) {
    float angle1 = (2.0f * M_PI * i) / points;
    float angle2 = (2.0f * M_PI * (i + 1)) / points;
    int x1 = cx + (int)(radius * cos(angle1));
    int y1 = cy + (int)(radius * sin(angle1));
    int x2 = cx + (int)(radius * cos(angle2));
    int y2 = cy + (int)(radius * sin(angle2));
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
  }
}

static void draw_silver_coin(SDL_Renderer *renderer, int centerX, int centerY) {
  const int radius = 20; // 40px diameter
  const int steps = 20;

  // Radial gradient from light center to darker edge
  for (int i = 0; i < steps; ++i) {
    float t = (float)i / (steps - 1);
    Uint8 shade = (Uint8)(200 + 55 * (1.0f - t));
    SDL_Color color = {shade, shade, shade, 255};
    draw_filled_circle(renderer, centerX, centerY, radius - i, color);
  }

  // Specular highlight
  SDL_Color highlight = {255, 255, 255, 150};
  draw_filled_circle(renderer, centerX - radius / 3, centerY - radius / 3, radius / 5, highlight);

  // Outline
  SDL_Color outline = {100, 100, 100, 255};
  draw_circle_outline(renderer, centerX, centerY, radius, outline);
}

static void render_card(game_state_t *game_state, SDL_Renderer *renderer, TTF_Font *font,
                        const int card_n, const uint8_t id, const int card_x, const int card_y) {
  SDL_Color textColor;
  char text[8] = {0};
  const char *face = get_card_face_str(game_state->player[id].hand.card[card_n].face_val);
  const char *suit = get_card_unicode_suit(game_state->player[id].hand.card[card_n]);
  snprintf(text, sizeof(text), "%s%s", face, suit);

  if (game_state->player[id].hand.card[card_n].suit == HEARTS ||
      game_state->player[id].hand.card[card_n].suit == DIAMONDS) {
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

void run_sdl_loop(game_state_t *game_state, struct sdl_context_t *sdl_context, struct font_t *font,
                  TCPsocket client_socket, SDLNet_SocketSet socket_set, const uint8_t my_id) {

  const struct pos_t player_pos[MAX_PLAYERS] = {
      // P0: bottom center
      {.x = sdl_context->window_width / 3, .y = sdl_context->window_height * 0.8},

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
    CHECK,
    FOLD,
    RAISE,
    CALL,
    ACTIONS_NUM,
  };

  const char *action[] = {
      [BET] = "Bet", [CHECK] = "Pass", [FOLD] = "Fold", [RAISE] = "Raise", [CALL] = "Call",
  };

  int x_offset = 100;
  struct button_t action_button[ACTIONS_NUM];
  for (int i = 0; i < ACTIONS_NUM; i++) {
    struct pos_t butt_pos = {x_offset += 130, sdl_context->win_center.y + 20};
    action_button[i] =
        create_button(action[i], sdl_context->renderer, &butt_pos, font->fonts[OTHER]);
  }

  int card_width = 80, card_height = 50;

  struct player_list_t *active_players = NULL;
  struct player_list_t *dealer = NULL;
  int running = 1;
  bool cards_dealt = false;
  while (running) {
    if (recv_game_state(client_socket, socket_set, game_state) != 0)
      running = false;
    // fprintf(stderr, "turn_id: %d\n", game_state->turn_id);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      int mx = event.button.x;
      int my = event.button.y;
      for (int i = 0; i < ACTIONS_NUM; i++) {
        action_button[i].enabled = true;
        action_button[i].hovered = SDL_PointInRect(&(SDL_Point){mx, my}, &action_button[i].rect);
      }
      if (event.type == SDL_QUIT) {
        running = 0;
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (game_state->turn_id == my_id) {
          if (point_in_rect(mx, my, &action_button[BET].rect)) {
            puts("sending bet");
            if (send_player_action(client_socket, ACTION_BET, 500) != 0)
              fprintf(stderr, "Failed to send bet\n");
          } else if (point_in_rect(mx, my, &action_button[FOLD].rect)) {
            puts("folding");
            if (send_player_action(client_socket, ACTION_FOLD, 0) != 0)
              fprintf(stderr, "Failed to fold\n");
          } else if (point_in_rect(mx, my, &action_button[CHECK].rect)) {
            puts("checking");
            if (send_player_action(client_socket, ACTION_CHECK, 0) != 0)
              fprintf(stderr, "Failed to check\n");
          } else if (point_in_rect(mx, my, &action_button[RAISE].rect)) {
            puts("raising");
            if (send_player_action(client_socket, ACTION_RAISE, 500) != 0)
              fprintf(stderr, "Failed to raise\n");
          } else if (point_in_rect(mx, my, &action_button[CALL].rect)) {
            puts("calling");
            if (send_player_action(client_socket, ACTION_CALL, 0) != 0)
              fprintf(stderr, "Failed to call\n");
          }
        }
      }
    }
    clear_screen(sdl_context->renderer);
    if (game_state->at_menu) {
      if (menu_display_game_choices(client_socket, socket_set, my_id, game_state, sdl_context,
                                    font) != 0) {
        running = false;
      } else {
        continue;
      }
    } else {
      if (!active_players) {
        active_players = create_player_list(game_state);
        if (!active_players)
          exit(EXIT_FAILURE);
        dealer = active_players;

        fprintf(stderr, "active_player id: %d\n", active_players->id);
        fprintf(stderr, "active_player id: %d\n", active_players->next->id);
      }
      for (int card_n = 0; card_n < HAND_SIZE; ++card_n) {
        do {
          int id = active_players->id;
          // fprintf(stderr, "id: %d\n", id);
          // Show each card that has been dealt
          int card_x = player_pos[id].x + card_n * (80 + 10);
          int card_y = player_pos[id].y;

          // Draw white card box
          SDL_Rect card_rect = {card_x, card_y, card_width, card_height};
          if (is_dh_card_null(game_state->player[id].hand.card[card_n]) == false) {
            SDL_SetRenderDrawColor(sdl_context->renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(sdl_context->renderer, &card_rect);
            SDL_SetRenderDrawColor(sdl_context->renderer, 0, 0, 0, 255);
            SDL_RenderDrawRect(sdl_context->renderer, &card_rect);
          }

          if (is_dh_card_back(game_state->player[id].hand.card[card_n]))
            draw_card_back_pattern(sdl_context->renderer, &card_rect);
          else if (is_dh_card_null(game_state->player[id].hand.card[card_n]) == false)
            render_card(game_state, sdl_context->renderer, font->fonts[CARD], card_n, id, card_x,
                        card_y);

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

          active_players = active_players->next;
        } while (active_players != dealer);
      }

      if (game_state->round_over)
        cards_dealt = false;
      struct player_list_t *ptr = active_players;
      if (game_state->winner_declared) {
        do {
          if (game_state->player[ptr->id].winner == true)
            break;

          ptr = ptr->next;

        } while (ptr != active_players);

        char winner_text[512] = {0};
        snprintf(winner_text, sizeof winner_text, "%s wins with %s",
                 game_state->player[ptr->id].name,
                 pokeval_ranks[pokeval_evaluate_hand(game_state->player[ptr->id].hand)]);
        SDL_Rect dest = {sdl_context->win_center.x, sdl_context->win_center.y - 50, 80, 20};
        render_text_plain(sdl_context->renderer, font->fonts[OTHER], winner_text,
                          get_color(COLOR_BLACK), &dest);
      } else {
        if (game_state->turn_id == my_id) {
          if (game_state->total_bets_plus_raises == 0 && !game_state->player[my_id].has_checked) {
            render_button(&action_button[BET]);
            render_button(&action_button[CHECK]);
            render_button(&action_button[FOLD]);
          } else {
            if (game_state->player[my_id].total_paid != game_state->total_bets_plus_raises) {
              render_button(&action_button[CALL]);
              render_button(&action_button[RAISE]);
              render_button(&action_button[FOLD]);
            }
          }
        }
      }

      cards_dealt = true;
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "pot: %d", game_state->pot);
      SDL_Color black = {0, 0, 0, 255};
      render_text_centered(sdl_context->renderer, font->fonts[OTHER], buffer, black,
                           sdl_context->win_center);

      do {
        int id = active_players->id;
        struct pos_t coin_pos = {.x = player_pos[id].x + (card_width * 1.2),
                                 .y = player_pos[id].y - (card_height * 0.9)};
        draw_silver_coin(sdl_context->renderer, coin_pos.x, coin_pos.y);
        char coins_text[24] = {0};
        snprintf(coins_text, sizeof coins_text, "= %d", game_state->player[id].coins);
        SDL_Rect dest = {coin_pos.x + 30, coin_pos.y - 20, 40, 20};
        render_text_plain(sdl_context->renderer, font->fonts[OTHER], coins_text,
                          get_color(COLOR_BLACK), &dest);

        // TODO: get the sizeof game_state_t [name]
        char name_text[512] = {0};
        snprintf(name_text, sizeof name_text, "%s", game_state->player[id].name);
        SDL_Rect dest_name = {player_pos[id].x + 30, player_pos[id].y + (card_height * 1.2), 40,
                              20};
        render_text_plain(sdl_context->renderer, font->fonts[OTHER], name_text,
                          get_color(COLOR_BLACK), &dest_name);

        active_players = active_players->next;
      } while (active_players != dealer);
    }

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }
  free_player_list(active_players);

  // Mix_FreeChunk(card_sound);
  // Mix_CloseAudio();
}
