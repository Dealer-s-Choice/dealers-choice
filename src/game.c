/*
 game.c
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

// What's the max this needs to be to support the unicode suit symbol?
#define SIZEOF_CARD_TEXT 20

#define CARD_DEAL_DELAY 50

Player_t *get_next_player(Player_t *players_array, int cur) {
  int start = cur;
  do {
    cur = (cur + 1) % MAX_PLAYERS;
    if (players_array[cur].id != -1)
      return &players_array[cur];
  } while (cur != start);

  return NULL; // No other active player found
}

static int8_t send_game_select(TCPsocket sock, uint8_t game_type) {
  uint8_t buffer[3];
  buffer[0] = (MSG_GAME_SELECT >> 8) & 0xFF;
  buffer[1] = (MSG_GAME_SELECT) & 0xFF;
  buffer[2] = game_type;

  return send_all_tcp(sock, buffer, sizeof(buffer));
}

// These two buttons for creating the buttons are mostly identical. In the future,
// they can be changed so there are some differences if desired. Otherwise,
// they'll be merged, and some of the values, such as the colors, will be passed
// as arguments.
static Button_t create_button(const char *text, SDL_Renderer *renderer, SDL_Point *pos,
                              TTF_Font *font) {
  Button_t button = {
      .text = text,
      .renderer = renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = {pos->x, pos->y, 120, 40},
      .font = font,
      .hovered = false,
      .enabled = true,
  };
  return button;
}

static Button_t create_game_choice_button(const char *text, SDL_Renderer *renderer, SDL_Rect rect,
                                          TTF_Font *font) {
  Button_t button = {
      .text = text,
      .renderer = renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = rect,
      .font = font,
      .hovered = false,
      .enabled = false,
  };
  return button;
}

const GameChoice_t game_choices[] = {
    {FIVE_CARD_DRAW, "5-card draw", 0x01, game_five_card_draw, 2, 1},
    {FIVE_CARD_DOUBLE_DRAW, "5-card double draw", 0x02, game_five_card_draw, 3, 2},
    {FIVE_CARD_STUD, "5-card stud", 0x03, game_five_card_stud, 4, 0}};

const GameChoice_t *find_game_choice_by_type(const uint8_t type) {
  for (size_t i = 0; i < sizeof(game_choices) / sizeof(game_choices[0]); ++i) {
    if (game_choices[i].game_type == type) {
      return &game_choices[i];
    }
  }
  return NULL; // Not found
}

void render_project_link(SDL_Renderer *renderer, TTF_Font *font, SDL_Rect *rect,
                         const bool hovered) {
  SDL_Color text_color = (hovered) ? get_color(COLOR_YELLOW) : get_color(COLOR_BLACK);
  TTF_SetFontStyle(font, TTF_STYLE_UNDERLINE);

  const char *ptr = &DEALERSCHOICE_URL[sizeof("https://") - 1];
  SDL_Surface *surface = TTF_RenderText_Solid(font, ptr, text_color);
  if (!surface) {
    SDL_Log("Failed to render text surface: %s", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    SDL_Log("Failed to create texture from surface: %s", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  rect->w = surface->w;
  rect->h = surface->h;

  SDL_FreeSurface(surface);

  if (!hovered)
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
  else
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

  SDL_RenderFillRect(renderer, rect);

  SDL_RenderCopy(renderer, texture, NULL, rect);
  SDL_DestroyTexture(texture);
}

static int menu_display_game_choices(TCPsocket client_socket, SDLNet_SocketSet socket_set,
                                     const int8_t my_id, GameState_t *game_state,
                                     ClientState_t *client_state, SdlContext_t *sdl_context,
                                     Font_t *font) {
  int button_height = 40;
  int y_offset = 160;
  Button_t game_choice_button[MAX_CHOICES];
  for (int i = 0; i < MAX_CHOICES; i++) {
    // TODO: Center
    // Using the strlen will expand the background to accomodate the text string (foreground)
    // but the buttons are not centered
    SDL_Rect rect = {100, y_offset, strlen(game_choices[i].str) * 16, button_height};

    game_choice_button[i] = create_game_choice_button(game_choices[i].str, sdl_context->renderer,
                                                      rect, font->fonts[OTHER]);
    y_offset += button_height * 1.1;
  }

  bool link_hovered;
  bool running = true;

  while (running && game_state->at_menu) {
    if (recv_game_state(client_socket, socket_set, game_state, client_state, my_id) == RECV_ERROR)
      return RECV_ERROR;

    SDL_Rect link_rect = {sdl_context->win_center.x + 50, sdl_context->window_height - 40,
                          sizeof(DEALERSCHOICE_URL) * 8, 30};

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      for (int i = 0; i < MAX_CHOICES; i++) {
        game_choice_button[i].enabled = (game_state->dealer_id == my_id);
        game_choice_button[i].hovered = SDL_PointInRect(&mouse_pos, &game_choice_button[i].rect);
      }
      link_hovered = SDL_PointInRect(&mouse_pos, &link_rect);
      if (e.type == SDL_QUIT) {
        return -1;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        for (int i = 0; i < MAX_CHOICES; i++) {
          if (SDL_PointInRect(&mouse_pos, &game_choice_button[i].rect) &&
              game_state->dealer_id == my_id) {
            if (send_game_select(client_socket, game_choices[i].game_type) == 0) {
              printf("Game type sent: %s", game_choices[i].str);
              running = false;
              break;
            } else {
              return -1;
            }
          }
        }
        if (link_hovered && e.button.button == SDL_BUTTON_LEFT)
          if (SDL_OpenURL(DEALERSCHOICE_URL) == -1)
            fputs(SDL_GetError(), stderr);
      }
    }

    // Clear screen
    clear_screen(sdl_context->renderer);

    for (int i = 0; i < MAX_CHOICES; i++)
      render_button(&game_choice_button[i]);

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

    render_project_link(sdl_context->renderer, font->fonts[LINK], &link_rect, link_hovered);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  return RECV_SUCCESS;
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

static int8_t send_discards_request_new_cards(TCPsocket sock, const uint8_t *discard_indices,
                                              uint8_t count) {
  if (count > 4)
    return -1;

  uint8_t buffer[7] = {0};
  buffer[0] = (MSG_DRAW_REQUEST >> 8) & 0xFF;
  buffer[1] = (MSG_DRAW_REQUEST) & 0xFF;
  buffer[2] = count;

  for (int i = 0; i < 4; ++i)
    buffer[3 + i] = (i < count) ? discard_indices[i] : 0xFF;

  return send_all_tcp(sock, buffer, sizeof(buffer));
}

bool is_dh_card_back(DH_Card a) {
  return a.face_val == DH_card_back.face_val && a.suit == DH_card_back.suit;
}

bool is_dh_card_null(DH_Card a) {
  return a.face_val == DH_card_null.face_val && a.suit == DH_card_null.suit;
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

typedef struct {
  char text[SIZEOF_CARD_TEXT];
  SDL_Color textColor;
  SDL_Renderer *renderer;
  // SDL_Color bg_color;
  // SDL_Color fg_color;
  SDL_Rect rect;
  bool hovered, selected, is_back, is_null;
} CardContext_t;

static void render_card(CardContext_t *context, TTF_Font *font) {
  // printf("%d\n", __LINE__);
  if (context->is_back) {
    draw_card_back_pattern(context->renderer, &context->rect);
    return;
  } else if (context->is_null)
    return;
  // Draw white card box
  SDL_SetRenderDrawColor(context->renderer, 255, 255, 255, 255);
  SDL_RenderFillRect(context->renderer, &context->rect);

  // Highlight hovered card for the local player (draw after card background)
  if (context->hovered) {
    SDL_SetRenderDrawBlendMode(context->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(context->renderer, 255, 255, 128, 96); // translucent yellow
    SDL_RenderFillRect(context->renderer, &context->rect);
    SDL_SetRenderDrawBlendMode(context->renderer, SDL_BLENDMODE_NONE);
  }

  // Draw thick lightgrey border if selected
  if (context->selected) {
    SDL_SetRenderDrawColor(context->renderer, 200, 200, 200, 255); // light grey
    int thickness = 4;
    for (int i = 0; i < thickness; ++i) {
      SDL_Rect border = {context->rect.x - i, context->rect.y - i, context->rect.w + 2 * i,
                         context->rect.h + 2 * i};
      SDL_RenderDrawRect(context->renderer, &border);
    }
  }

  // Draw card border
  SDL_SetRenderDrawColor(context->renderer, 0, 0, 0, 255);
  SDL_RenderDrawRect(context->renderer, &context->rect);

  SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, context->text, context->textColor);
  if (!textSurface) {
    fprintf(stderr, "TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
    exit(EXIT_FAILURE);
  }

  SDL_Texture *textTexture = SDL_CreateTextureFromSurface(context->renderer, textSurface);
  if (!textTexture) {
    fprintf(stderr, "SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
    SDL_FreeSurface(textSurface);
    exit(EXIT_FAILURE);
  }

  SDL_Rect textRect = {context->rect.x + (80 - textSurface->w) / 2,
                       context->rect.y + (50 - textSurface->h) / 2, textSurface->w, textSurface->h};

  SDL_RenderCopy(context->renderer, textTexture, NULL, &textRect);
  SDL_FreeSurface(textSurface);
  SDL_DestroyTexture(textTexture);
}

static void create_card_context(CardContext_t card_context[MAX_PLAYERS][HAND_SIZE],
                                const int start_i, Player_t *players_array,
                                const SDL_Point *player_pos, SDL_Renderer *renderer) {
  memset(card_context, 0, sizeof(CardContext_t) * MAX_PLAYERS * HAND_SIZE);
  Player_t *turn = &players_array[start_i];
  Player_t *starting_turn = turn;
  do {
    CardContext_t context = {
        .renderer = renderer,
        .hovered = false,
        .selected = false,
    };
    for (int card_n = 0; card_n < HAND_SIZE; card_n++) {
      // printf("%d\n", __LINE__);
      const int id = turn->id;
      DH_Card *card = &(turn->hand.card)[card_n];
      const SDL_Point card_pos = {
          player_pos[id].x + card_n * (80 + 10),
          player_pos[id].y,
      };
      SDL_Rect rect = {card_pos.x, card_pos.y, 80, 50};
      context.rect = rect;

      SDL_Color textColor = {0, 0, 0, 0};
      context.textColor = textColor;

      context.is_back = is_dh_card_back(*card);
      context.is_null = is_dh_card_null(*card);
      if (!context.is_back && !context.is_null) {
        const char *face = DH_get_card_face_str(card->face_val);
        const char *suit = DH_get_card_unicode_suit(*card);
        context.textColor = (card->suit == DH_SUIT_HEARTS || card->suit == DH_SUIT_DIAMONDS)
                                ? get_color(COLOR_RED)
                                : get_color(COLOR_BLACK);
        snprintf(context.text, sizeof(context.text), "%s%s", face, suit);
        if (strlen(context.text) == 0) {
          fprintf(stderr, "%s:String length 0\n", __func__);
          exit(EXIT_FAILURE);
        }
      }
      card_context[id][card_n] = context;
    }
  } while ((turn = get_next_player(players_array, turn->id)) != starting_turn);
}

void run_sdl_loop(GameState_t *game_state, ClientState_t *client_state, SdlContext_t *sdl_context,
                  Font_t *font, TCPsocket client_socket, SDLNet_SocketSet socket_set,
                  const uint8_t my_id) {

  const SDL_Point player_pos[MAX_PLAYERS] = {
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

  char status_msg[16][LEN_STATUS_STR];

  enum {
    BET,
    CHECK,
    FOLD,
    RAISE,
    CALL,
    DISCARD,
    MAX_ACTIONS,
  };

  const char *action[] = {[BET] = "Bet",     [CHECK] = "Check", [FOLD] = "Fold",
                          [RAISE] = "Raise", [CALL] = "Call",   [DISCARD] = "Discard"};

  int x_offset = 100;
  Button_t action_button[MAX_ACTIONS];
  for (int i = 0; i < MAX_ACTIONS; i++) {
    SDL_Point butt_pos = {x_offset += 130, sdl_context->win_center.y + 20};
    action_button[i] =
        create_button(action[i], sdl_context->renderer, &butt_pos, font->fonts[OTHER]);
  }

  int card_width = 80, card_height = 50;

  CardContext_t card_context[MAX_PLAYERS][HAND_SIZE];

  int running = 1;
  bool cards_dealt = false;
  bool cards_created = false;
  // Uint32 timer_start;

  Player_t *players_array = game_state->player;
  Player_t *turn = NULL;
  Player_t *starting_turn = NULL;
  while (running) {
    ERecvStatus_t recv_status =
        recv_game_state(client_socket, socket_set, game_state, client_state, my_id);
    // printf("%d\n", __LINE__);
    if (recv_status == RECV_ERROR)
      running = false;
    else if (recv_status == RECV_SUCCESS)
      cards_created = false;

    if (game_state->at_menu) {
      if (menu_display_game_choices(client_socket, socket_set, my_id, game_state, client_state,
                                    sdl_context, font) != RECV_SUCCESS) {
        running = false;
      } else {
        // timer_start = 0;
        cards_dealt = false;
        starting_turn = &game_state->player[game_state->turn_id];
        memset(client_state, 0, sizeof *client_state);
        memset(status_msg, 0, sizeof status_msg);
        continue;
      }
    } else {
      // if (timer_start == 0 && game_state->turn_id == my_id)
      // SDL_GetTicks();

      turn = &game_state->player[game_state->turn_id];
      // printf("turn id: %d\n", game_state->turn_id);

      if (strcmp(client_state->server_status_str, status_msg[15]) != 0) {
        // Shift messages up by one slot: [1]..[15] → [0]..[14]
        memmove(&status_msg[0], &status_msg[1], sizeof(status_msg[0]) * (16 - 1));

        // Copy new message to the bottom
        snprintf(status_msg[15], sizeof(client_state->server_status_str), "%s",
                 client_state->server_status_str);
      }

      // debug_print_cards(&game_state->player[0].hand);
      // debug_print_cards(&game_state->player[1].hand);

      // Only create the cards but doesn't render them yet.
      // This is done when the client receives new data, not every iteration
      // of the loop. The flag gets set above. TODO: It should only happen if
      // the server sends new information about cards.
      if (!cards_created) {
        // printf("%d\n", __LINE__);
        create_card_context(card_context, starting_turn->id, players_array, player_pos,
                            sdl_context->renderer);
        cards_created = true;
      }

      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        SDL_Point mouse_pos = {event.button.x, event.button.y};
        for (int card_n = 0; card_n < HAND_SIZE; card_n++) {
          DH_Card *card = &game_state->player[my_id].hand.card[card_n];
          if (!is_dh_card_null(*card) || !is_dh_card_null(*card)) {
            card_context[my_id][card_n].hovered =
                SDL_PointInRect(&mouse_pos, &card_context[my_id][card_n].rect);
            if (card_context[my_id][card_n].hovered && event.type == SDL_MOUSEBUTTONDOWN &&
                client_state->do_discard_draw) {
              // select or deselect when clicked
              bool *selected = &card_context[my_id][card_n].selected;
              *selected = !(*selected);

              // Update counter
              if (*selected) {
                client_state->n_cards_selected++;
              } else {
                client_state->n_cards_selected--;
              }
              // printf("n_selected: %d\n", client_state->n_cards_selected);
            }
            // If the mouse is at the location, there's no need to iterate through the rest
            // of the cards.
            if (card_context[my_id][card_n].hovered)
              break;
          }
        }
        for (int i = 0; i < MAX_ACTIONS; i++) {
          action_button[i].hovered = SDL_PointInRect(&mouse_pos, &action_button[i].rect);
        }
        if (event.type == SDL_QUIT) {
          running = false;
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
          if (game_state->turn_id == my_id && !client_state->do_discard_draw) {
            // TODO: use existing array (or modify it) to loop through each action
            if (SDL_PointInRect(&mouse_pos, &action_button[BET].rect)) {
              puts("sending bet");
              if (send_player_action(client_socket, ACTION_BET, 500) != 0)
                fprintf(stderr, "Failed to send bet\n");
            } else if (SDL_PointInRect(&mouse_pos, &action_button[FOLD].rect)) {
              puts("folding");
              if (send_player_action(client_socket, ACTION_FOLD, 0) != 0)
                fprintf(stderr, "Failed to fold\n");
            } else if (SDL_PointInRect(&mouse_pos, &action_button[CHECK].rect)) {
              puts("checking");
              if (send_player_action(client_socket, ACTION_CHECK, 0) != 0)
                fprintf(stderr, "Failed to check\n");
            } else if (SDL_PointInRect(&mouse_pos, &action_button[RAISE].rect)) {
              puts("raising");
              if (send_player_action(client_socket, ACTION_RAISE, 500) != 0)
                fprintf(stderr, "Failed to raise\n");
            } else if (SDL_PointInRect(&mouse_pos, &action_button[CALL].rect)) {
              puts("calling");
              if (send_player_action(client_socket, ACTION_CALL, 0) != 0)
                fprintf(stderr, "Failed to call\n");
            }
          } else if (action_button[DISCARD].enabled &&
                     SDL_PointInRect(&mouse_pos, &action_button[DISCARD].rect)) {
            puts("discarding");
            // Although the maximum allowed discards for 5 card draw can never
            // exceed 4, we need an array size of HAND_SIZE in case they select
            // all 5. However, the player will be required to have < HAND_SIZE selected
            // to actually perform the discard.
            uint8_t discard_indices[HAND_SIZE] = {0};
            uint8_t discard_count = 0;

            for (int i = 0; i < HAND_SIZE; i++) {
              if (!card_context[my_id][i].selected)
                continue;
              discard_indices[discard_count++] = i;
            }

            // Reset the flag that's used to indicate to the client it's their turn to draw
            client_state->do_discard_draw = false;

            // The server normally sets this, and the client receives it, during game broadcast
            // game_state->turn_id = -1;

            if (send_discards_request_new_cards(client_socket, discard_indices, discard_count) != 0)
              fprintf(stderr, "Failed to send discards\n");
            else
              puts("Discards sent");
          }
        }
      }
      clear_screen(sdl_context->renderer);

      // for (size_t i = 0; i < sizeof(status_msg) / sizeof(status_msg[0][0]); i++) {
      for (int i = 0; i < 16; i++) {
        char tmp[sizeof(status_msg[0])];
        snprintf(tmp, sizeof tmp, "%s", status_msg[i]);
        // TODO: The x & y offsets need to be scaled somehow, not hard-coded
        SDL_Rect text_pos = {sdl_context->win_center.x + 100, 20 * i + 5, 0, 0};
        render_text_plain(sdl_context->renderer, font->fonts[STATUS_MSG], tmp,
                          get_color(COLOR_BLACK), &text_pos);
        // printf("status_msg[%zd]: %s\n", i, status_msg[i]);
      }

      for (int card_n = 0; card_n < HAND_SIZE; ++card_n) {
        turn = starting_turn;
        do {
          // printf("%d\n", __LINE__);
          render_card(&card_context[turn->id][card_n], font->fonts[CARD]);

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
            // fprintf(stderr, "%s:turn->id: %d\n", __func__, turn->id);
          }
        } while ((turn = get_next_player(players_array, turn->id)) != starting_turn);
      }
      // if (recv_game_state(client_socket, socket_set, game_state) == RECV_ERROR)
      // running = false;

      // printf("%d\n", __LINE__);

      if (game_state->winner_declared) {
        turn = starting_turn;
        do {
          // printf("%d\n", __LINE__);
          if (turn->winner == true) {
            break;
          }
        } while ((turn = get_next_player(players_array, turn->id)) != starting_turn);

        // char winner_text[sizeof(turn->name) + 64] = {0};
        // if (game_state->player_count > 1)
        // snprintf(winner_text, sizeof winner_text, "%s wins with %s", turn->name,
        // pokeval_ranks[pokeval_evaluate_hand(turn->hand)]);
        // else
        // snprintf(winner_text, sizeof winner_text, "%s wins", turn->name);

        // SDL_Rect dest = {sdl_context->win_center.x, sdl_context->win_center.y - 50, 80, 20};
        // render_text_plain(sdl_context->renderer, font->fonts[OTHER], winner_text,
        // get_color(COLOR_BLACK), &dest);
      } else {
        if (client_state->do_discard_draw) {
          for (int i = 0; i < HAND_SIZE; i++)
            if (turn->hand.card[i].face_val == DH_CARD_ACE) {
              client_state->has_ace = true;
              break;
            }
          uint8_t max_allowed = client_state->has_ace ? 4 : 3;
          action_button[DISCARD].enabled = client_state->n_cards_selected <= max_allowed;
          if (!action_button[DISCARD].enabled) {
            char tmp[50] = {0};
            snprintf(tmp, sizeof(tmp), "You may only discard a maximum of %d cards", max_allowed);
            render_text_plain(sdl_context->renderer, font->fonts[OTHER], tmp,
                              get_color(COLOR_WHITE),
                              &(SDL_Rect){action_button->rect.x, action_button->rect.y + 50, 0, 0});
          }
          render_button(&action_button[DISCARD]);
        } else if (game_state->turn_id == my_id) {
          if (game_state->total_bets_plus_raises == 0 && !game_state->player[my_id].has_checked) {
            render_button(&action_button[BET]);
            render_button(&action_button[CHECK]);
            render_button(&action_button[FOLD]);
          } else if (game_state->player[my_id].total_paid != game_state->total_bets_plus_raises) {
            render_button(&action_button[CALL]);
            render_button(&action_button[RAISE]);
            render_button(&action_button[FOLD]);
          }
        } else {

          // Uint32 now = SDL_GetTicks();
          // char elapsed[8] = {0};
          // snprintf(elapsed, sizeof(elapsed), "%d", (timer_start + game_state->action_time_out_ms
          // - now) / 1000); render_text_plain(sdl_context->renderer, font->fonts[OTHER], elapsed,
          // get_color(COLOR_WHITE), &(SDL_Rect){sdl_context->window_width - 60,
          // sdl_context->window_height - 60, 0, 0});
        }
      }

      cards_dealt = true;
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "pot: %d", game_state->pot);
      SDL_Color black = {0, 0, 0, 255};
      render_text_centered(sdl_context->renderer, font->fonts[OTHER], buffer, black,
                           sdl_context->win_center);

      do {
        // printf("%d\n", __LINE__);
        int id = turn->id;
        SDL_Point coin_pos = {.x = player_pos[id].x + (card_width * 1.2),
                              .y = player_pos[id].y - (card_height * 0.9)};
        draw_silver_coin(sdl_context->renderer, coin_pos.x, coin_pos.y);
        char coins_text[24] = {0};
        snprintf(coins_text, sizeof coins_text, "= %d", turn->coins);
        SDL_Rect dest = {coin_pos.x + 30, coin_pos.y - 20, 40, 20};
        render_text_plain(sdl_context->renderer, font->fonts[OTHER], coins_text,
                          get_color(COLOR_BLACK), &dest);

        char name_text[sizeof(turn->name)] = {0};
        snprintf(name_text, sizeof name_text, "%s", turn->name);
        SDL_Rect dest_name = {player_pos[id].x + 30, player_pos[id].y + (card_height * 1.2), 40,
                              20};
        render_text_plain(sdl_context->renderer, font->fonts[OTHER], name_text,
                          get_color(COLOR_BLACK), &dest_name);

      } while ((turn = get_next_player(players_array, turn->id)) != starting_turn);
    }

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }
  // Mix_FreeChunk(card_sound);
  // Mix_CloseAudio();
}
