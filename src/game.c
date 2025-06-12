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
#include <pcg_basic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "game.h"

// Build fails using gcc on Ubuntu 24.04 (and maybe others) without this
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// What's the max this needs to be to support the unicode suit symbol?
#define SIZEOF_CARD_TEXT 20

#define CARD_DEAL_DELAY 50

#define MAX_POT_COINS 50

const SDL_Rect card_area = {0, 0, 80, 50};

static bool is_valid_player(const Player_t *p, bool want_all_clients) {
  return p->id != -1 && (want_all_clients || p->in);
}

static Player_t *get_next_player_real(Player_t *players_array, int cur, bool want_all_clients) {
  if (cur < 0) {
    fprintf(stderr, "%s: 'cur' may not be a negative value.\n", __func__);
    exit(EXIT_FAILURE);
  }

  int i = (cur + 1) % MAX_PLAYERS;

  while (i != cur) {
    // fprintf(stderr, "i: %d\n", i);
    if (is_valid_player(&players_array[i], want_all_clients)) {
      return &players_array[i];
    }
    i = (i + 1) % MAX_PLAYERS;
  }

  // fprintf(stderr, "i: %d\n", i);
  // Final fallback: check starting index
  if (is_valid_player(&players_array[cur], want_all_clients)) {
    return &players_array[cur];
  }

  fputs("No valid players found\n", stderr);
  exit(EXIT_FAILURE);
}

Player_t *get_next_player(Player_t *players_array, int cur) {
  const bool want_all_clients = false;
  return get_next_player_real(players_array, cur, want_all_clients);
}

Player_t *get_next_connected_client(Player_t *players_array, int cur) {
  const bool want_all_clients = true;
  return get_next_player_real(players_array, cur, want_all_clients);
}

int8_t send_game_select(TCPsocket sock, uint8_t game_type) {
  uint8_t buffer[3];
  buffer[0] = (MSG_GAME_SELECT >> 8) & 0xFF;
  buffer[1] = (MSG_GAME_SELECT) & 0xFF;
  buffer[2] = game_type;

  const GameChoice_t *choice = find_game_choice_by_type(game_type);
  int r = send_all_tcp(sock, buffer, sizeof(buffer));
  if (r == 0) {
    fprintf(stderr, "Game type sent: %s\n", choice->str);
    return r;
  }

  fprintf(stderr, "Game type failed to send: %s\n", choice->str);
  return r;
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
      .selected = false,
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
      .selected = false,
  };
  return button;
}

const GameChoice_t game_choices[] = {
    {FIVE_CARD_DRAW, "5-card draw", 0x01, game_five_card_draw, 2, 1},
    {FIVE_CARD_DOUBLE_DRAW, "5-card double draw", 0x02, game_five_card_draw, 3, 2},
    {FIVE_CARD_STUD, "5-card stud", 0x03, game_five_card_stud, 4, 3},
    {FIVE_CARD_SHOWDOWN, "5-Card Showdown", 0x04, game_five_card_draw, 1, 0}};

const GameChoice_t *find_game_choice_by_type(const uint8_t type) {
  for (size_t i = 0; i < sizeof(game_choices) / sizeof(game_choices[0]); ++i) {
    if (game_choices[i].game_type == type) {
      return &game_choices[i];
    }
  }
  return NULL; // Not found
}

void render_link(Link_t *link) {
  link->rect.w *= strlen(link->url);
  SDL_Color text_color = (link->hovered) ? get_color(COLOR_BLUE) : get_color(COLOR_BLACK);
  TTF_SetFontStyle(link->font, TTF_STYLE_UNDERLINE);

  const char *ptr = &link->url[sizeof("https://") - 1];
  SDL_Surface *surface = TTF_RenderText_Solid(link->font, ptr, text_color);
  if (!surface) {
    SDL_Log("Failed to render text surface: %s", TTF_GetError());
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(link->renderer, surface);
  if (!texture) {
    SDL_Log("Failed to create texture from surface: %s", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  link->rect.w = surface->w;
  link->rect.h = surface->h;

  SDL_FreeSurface(surface);

  if (link->hovered)
    SDL_SetRenderDrawColor(link->renderer, 255, 255, 255, 255);
  else
    SDL_SetRenderDrawColor(link->renderer, 230, 245, 230, 255);

  SDL_RenderFillRect(link->renderer, &link->rect);

  SDL_RenderCopy(link->renderer, texture, NULL, &link->rect);
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
    SDL_Rect rect = {100, y_offset, strlen(game_choices[i].str) * 18, button_height};

    game_choice_button[i] = create_game_choice_button(game_choices[i].str, sdl_context->renderer,
                                                      rect, font->fonts[OTHER]);
    y_offset += button_height * 1.1;
  }

  bool running = true;
  // const char *link[] = { DEALERSCHOICE_URL, "https://matrix.to/#/#dealers-choice:matrix.org" };
  Link_t link[] = {
      {DEALERSCHOICE_URL, font->fonts[LINK], sdl_context->renderer,
       (SDL_Rect){sdl_context->win_center.x + 50, sdl_context->window_height - 40, 8, 30}, false},
      {"https://matrix.to/#/#dealers-choice:matrix.org", font->fonts[LINK], sdl_context->renderer,
       (SDL_Rect){20, sdl_context->window_height - 40, 8, 30}, false}};

  while (running && game_state->at_menu) {
    ERecvStatus_t recv_status =
        recv_game_state(client_socket, socket_set, game_state, client_state, my_id);
    if (recv_status == RECV_ERROR)
      return RECV_ERROR;
    // else if (recv_status == RECV_NOTHING)
    // fprintf(stderr, "%s: Received nothing\n", __func__);
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      for (int i = 0; i < MAX_CHOICES; i++) {
        game_choice_button[i].enabled = (game_state->dealer_id == my_id);
        game_choice_button[i].hovered = SDL_PointInRect(&mouse_pos, &game_choice_button[i].rect);
      }
      link[0].hovered = SDL_PointInRect(&mouse_pos, &link[0].rect);
      link[1].hovered = SDL_PointInRect(&mouse_pos, &link[1].rect);
      if (e.type == SDL_QUIT) {
        return -1;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        for (int i = 0; i < MAX_CHOICES; i++) {
          if (SDL_PointInRect(&mouse_pos, &game_choice_button[i].rect) &&
              game_state->dealer_id == my_id) {
            if (send_game_select(client_socket, game_choices[i].game_type) == 0) {
              running = false;
              break;
            } else {
              return -1;
            }
          }
        }
        for (size_t i = 0; i < sizeof link / sizeof link[0]; i++) {
          if (link[i].hovered && e.button.button == SDL_BUTTON_LEFT)
            if (SDL_OpenURL(link[i].url) == -1)
              fputs(SDL_GetError(), stderr);
        }
      }
    }

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

    Player_t *client = &game_state->player[my_id];
    Player_t *start = client;
    do {
      // fprintf(stderr, "%d\n", __LINE__);
      offset_y += 40;
      char tmp[sizeof(client->nick) + 20] = {0};
      snprintf(tmp, sizeof tmp, "%s%s", client->nick,
               game_state->dealer_id == client->id ? " (Dealer)" : "");
      SDL_Rect text_pos = {offset_x, offset_y, 0, 0};
      render_text_plain(sdl_context->renderer, font->fonts[OTHER], tmp, get_color(COLOR_WHITE),
                        &text_pos);
    } while ((client = get_next_connected_client(game_state->player, client->id)) != start);
    // fprintf(stderr, "%d\n", __LINE__);

    render_link(&link[0]);
    render_link(&link[1]);

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

int8_t send_player_action(TCPsocket sock, uint8_t action, uint32_t amount) {
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

  if (context->selected)
    mark_selected(context->renderer, &context->rect);

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
      if (!turn->in)
        memcpy(card, &DH_card_back, sizeof(DH_card_back));

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
  } while ((turn = get_next_connected_client(players_array, turn->id)) != starting_turn);
}

static pcg32_random_t rng;
static void pcg_srand_auto(void) {
  uint64_t initstate = time(NULL) ^ (intptr_t)&printf;
  uint64_t initseq = (intptr_t)&pcg_srand_auto;
  pcg32_srandom_r(&rng, initstate, initseq);
}

void run_sdl_loop(ClientState_t *client_state, SdlContext_t *sdl_context, Font_t *font,
                  TCPsocket client_socket, SDLNet_SocketSet socket_set, const uint8_t my_id,
                  Path_t *path) {
  Uint32 start_time = SDL_GetTicks(); // milliseconds
  const Uint32 timeout = 2000;        // 2 seconds
  const Uint32 retry_delay = 100;     // milliseconds per retry

  ERecvStatus_t recv_status;
  GameState_t game_state = {0};

  do {
    recv_status = recv_game_state(client_socket, socket_set, &game_state, client_state, my_id);

    if (recv_status == RECV_SUCCESS) {
      break;
    } else if (recv_status == RECV_ERROR) {
      fprintf(stderr, "Initial game state not received\n");
      exit(EXIT_FAILURE);
    }

    SDL_Delay(retry_delay);
  } while (SDL_GetTicks() - start_time < timeout);
  if (recv_status != RECV_SUCCESS)
    exit(EXIT_FAILURE);

  const SDL_Point player_pos[MAX_PLAYERS] = {
      // P0: bottom center
      {.x = sdl_context->window_width / 3, .y = sdl_context->window_height * 0.8},

      // P1: left, 1/3 down
      {.x = 20, .y = sdl_context->window_height / 3},

      // P2: top-left
      {.x = 20, .y = card_area.h * 3},

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
    CHECK,
    BET,
    FOLD,
    CALL,
    RAISE,
    DISCARD,
    MAX_ACTIONS,
  };

  const char *action[] = {[CHECK] = "Check", [BET] = "Bet",     [FOLD] = "Fold",
                          [CALL] = "Call",   [RAISE] = "Raise", [DISCARD] = "Discard"};

  const int start_x_offset = 100;
  int x_offset = start_x_offset;
  const int action_button_y = sdl_context->win_center.y;
  Button_t action_button[MAX_ACTIONS];
  for (int i = 0; i < MAX_ACTIONS; i++) {
    SDL_Point butt_pos = {x_offset += 130, action_button_y + 20};
    if (i == RAISE)
      butt_pos = (SDL_Point){action_button[BET].rect.x, action_button[BET].rect.y};
    else if (i == CALL)
      butt_pos = (SDL_Point){action_button[CHECK].rect.x, action_button[CHECK].rect.y};
    else if (i == DISCARD)
      butt_pos = (SDL_Point){action_button[BET].rect.x, action_button[BET].rect.y};
    action_button[i] =
        create_button(action[i], sdl_context->renderer, &butt_pos, font->fonts[OTHER]);
  }

  x_offset = start_x_offset + 150;
  const uint8_t n_amounts = 3;
  Button_t amount_button[n_amounts];
  const char *amount[] = {"100", "250", "500"};
  for (int i = 0; i < n_amounts; i++) {
    const uint8_t w = 60;
    const uint8_t space = 5;
    amount_button[i] = (Button_t){
        amount[i],
        sdl_context->renderer,
        get_color(COLOR_WHITE),
        get_color(COLOR_BROWN),
        (SDL_Rect){x_offset + (i * (w + space)), action_button_y + 80, w, 40},
        font->fonts[OTHER],
        false,
        true,
        false,
    };
  }
  amount_button[0].selected = true;

  CardContext_t card_context[MAX_PLAYERS][HAND_SIZE];

  int running = 1;
  bool cards_dealt = false;
  bool cards_created = false;
  Uint32 timer_start;
  int8_t save_turn_id;

  Player_t *players_array = game_state.player;
  Player_t *turn = NULL;
  Player_t *starting_turn = NULL;

  const char *coin_path = "48x48_1907_Saint_Gaudens_gold_coin.png";
  const char *subdir = "/images/";

  size_t req_len = strlen(path->data) + strlen(subdir) + strlen(coin_path) + 1;
  char *coin_location = calloc_wrap(req_len, 1);
  snprintf(coin_location, req_len, "%s%s%s", path->data, subdir, coin_path);
  SDL_Texture *coin_texture = load_texture(sdl_context->renderer, coin_location);
  free(coin_location);

  SDL_Point coin_coords[MAX_POT_COINS] = {0};
  uint8_t coins;
  pcg_srand_auto();

  while (running) {
    recv_status = recv_game_state(client_socket, socket_set, &game_state, client_state, my_id);
    // printf("%d\n", __LINE__);
    if (recv_status == RECV_ERROR)
      running = false;
    else if (recv_status == RECV_SUCCESS)
      cards_created = false;

    // printf("%d\n", __LINE__);

    if (game_state.at_menu) {
      if (menu_display_game_choices(client_socket, socket_set, my_id, &game_state, client_state,
                                    sdl_context, font) != RECV_SUCCESS) {
        running = false;
      } else {
        timer_start = SDL_GetTicks();
        cards_dealt = false;
        starting_turn = &game_state.player[game_state.turn_id];
        save_turn_id = game_state.turn_id;
        client_state->save_starting_turn_id = starting_turn->id;
        memset(client_state, 0, sizeof *client_state);
        client_state->selected_amount = atoi(amount[0]);
        memset(status_msg, 0, sizeof status_msg);
        memset(coin_coords, 0, sizeof(coin_coords));
        coins = 0;
        continue;
      }
    } else {
      // For cases when the client who was designated as starting_turn disconnects
      if (starting_turn->id == -1)
        starting_turn = get_next_player(players_array, client_state->save_starting_turn_id);

      turn = &game_state.player[game_state.turn_id];
      if (game_state.turn_id != save_turn_id) {
        save_turn_id = game_state.turn_id;
        timer_start = SDL_GetTicks();
      }

      // printf("turn id: %d\n", turn->id);

      if (strcmp(client_state->server_status_str, status_msg[15]) != 0) {
        // Shift messages up by one slot: [1]..[15] → [0]..[14]
        memmove(status_msg, status_msg + 1, sizeof(status_msg[0]) * 15);

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
      // printf("%d\n", __LINE__);

      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        SDL_Point mouse_pos = {event.button.x, event.button.y};
        for (int card_n = 0; card_n < HAND_SIZE; card_n++) {
          DH_Card *card = &turn->hand.card[card_n];
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
        for (int i = 0; i < n_amounts; i++) {
          amount_button[i].hovered = SDL_PointInRect(&mouse_pos, &amount_button[i].rect);
          if (amount_button[i].hovered && event.type == SDL_MOUSEBUTTONDOWN) {
            amount_button[i].selected = true;
            client_state->selected_amount = atoi(amount_button[i].text);
            break;
          }
        }
        if (event.type == SDL_QUIT) {
          running = false;
        } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN) {
          for (int i = 0; i < n_amounts; i++) {
            if (SDL_PointInRect(&mouse_pos, &amount_button[i].rect)) {
              // Deselect all buttons
              for (int j = 0; j < n_amounts; j++) {
                amount_button[j].selected = false;
              }
              // Select the clicked one
              amount_button[i].selected = true;
              break;
            }
          }
          if (game_state.turn_id == my_id && !client_state->do_discard_draw) {
            if ((game_state.total_bets_plus_raises == 0 && !turn->has_checked) ||
                game_state.player[my_id].total_paid != game_state.total_bets_plus_raises) {
              if (SDL_PointInRect(&mouse_pos, &action_button[FOLD].rect) ||
                  event.key.keysym.sym == SDLK_f) {
                puts("folding");
                if (send_player_action(client_socket, ACTION_FOLD, 0) != 0)
                  fprintf(stderr, "Failed to fold\n");
              }
            }
            if (game_state.total_bets_plus_raises == 0 && !turn->has_checked) {
              // TODO: use existing array (or modify it) to loop through each action
              if (SDL_PointInRect(&mouse_pos, &action_button[BET].rect) ||
                  event.key.keysym.sym == SDLK_b) {
                puts("sending bet");
                if (send_player_action(client_socket, ACTION_BET, client_state->selected_amount) !=
                    0)
                  fprintf(stderr, "Failed to send bet\n");
              } else if (SDL_PointInRect(&mouse_pos, &action_button[CHECK].rect) ||
                         event.key.keysym.sym == SDLK_c) {
                puts("checking");
                if (send_player_action(client_socket, ACTION_CHECK, 0) != 0)
                  fprintf(stderr, "Failed to check\n");
              }
            } else if (game_state.player[my_id].total_paid != game_state.total_bets_plus_raises) {
              if (SDL_PointInRect(&mouse_pos, &action_button[RAISE].rect) ||
                  event.key.keysym.sym == SDLK_r) {
                puts("raising");
                if (send_player_action(client_socket, ACTION_RAISE,
                                       client_state->selected_amount) != 0)
                  fprintf(stderr, "Failed to raise\n");
              } else if (SDL_PointInRect(&mouse_pos, &action_button[CALL].rect) ||
                         event.key.keysym.sym == SDLK_c) {
                puts("calling");
                if (send_player_action(client_socket, ACTION_CALL, 0) != 0)
                  fprintf(stderr, "Failed to call\n");
              }
            }
          } else if (action_button[DISCARD].enabled &&
                     (SDL_PointInRect(&mouse_pos, &action_button[DISCARD].rect) ||
                      event.key.keysym.sym == SDLK_d)) {
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
            else {
              puts("Discards sent");
              client_state->n_cards_selected = 0;
            }
          }
        }
      }

      clear_screen(sdl_context->renderer);

      if (game_state.pot > coins * 250 && coins < MAX_POT_COINS) {
        coin_coords[coins].x = sdl_context->win_center.x + pcg32_boundedrand_r(&rng, 250) - 150;
        coin_coords[coins].y = sdl_context->win_center.y + pcg32_boundedrand_r(&rng, 250) - 150;
        coins++;
      }

      for (int i = 0; i < coins; i++) {
        SDL_Rect coin_rect = {
            .x = coin_coords[i].x,
            .y = coin_coords[i].y,
            .w = 48,
            .h = 48,
        };
        SDL_RenderCopy(sdl_context->renderer, coin_texture, NULL, &coin_rect);
      }

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

      Player_t *player_ptr = starting_turn;
      for (int card_n = 0; card_n < HAND_SIZE; ++card_n) {
        do {
          // printf("%d\n", __LINE__);
          render_card(&card_context[player_ptr->id][card_n], font->fonts[CARD]);

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
        } while ((player_ptr = get_next_connected_client(players_array, player_ptr->id)) !=
                 starting_turn);
      }

      //// printf("%d\n", __LINE__);
      // printf("game_state.total_bets_plus_raises: %d\n",
      // game_state.total_bets_plus_raises == 0 && !turn->has_checked);
      // printf("turn->has_checked: %s, turn->id: %d, my_id: %d\n\n",
      // turn->has_checked ? "true" : "false", turn->id, my_id);

      if (game_state.winner_declared) {
        player_ptr = starting_turn;
        do {
          // printf("%d\n", __LINE__);
          if (player_ptr->winner == true) {
            break;
          }
        } while ((player_ptr = get_next_connected_client(players_array, player_ptr->id)) !=
                 starting_turn);

        // printf("%d\n", __LINE__);

      } else {
        Uint32 now = SDL_GetTicks();
        int32_t remaining_ms =
            (int32_t)(timer_start + game_state.action_time_out_ms) - (int32_t)now;

        if (remaining_ms > 0) {
          int elapsed = remaining_ms / 1000;
          char elapsed_str[8] = {0};
          snprintf(elapsed_str, sizeof(elapsed_str), "%d", elapsed);

          render_text_plain(
              sdl_context->renderer, font->fonts[OTHER], elapsed_str, get_color(COLOR_WHITE),
              &(SDL_Rect){sdl_context->window_width - 60, sdl_context->window_height - 60, 0, 0});
        }

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
        } else if (game_state.turn_id == my_id) {
          if (game_state.total_bets_plus_raises == 0 && !turn->has_checked) {
            render_button(&action_button[BET]);
            render_button(&action_button[CHECK]);
          } else if (game_state.player[my_id].total_paid != game_state.total_bets_plus_raises) {
            render_button(&action_button[CALL]);
            render_button(&action_button[RAISE]);
          }
          render_button(&action_button[FOLD]);
          for (int i = 0; i < n_amounts; i++)
            render_button(&amount_button[i]);
        } else {
        }
      }
      // printf("%d\n", __LINE__);

      cards_dealt = true;
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "pot: %d", game_state.pot);
      SDL_Color black = {0, 0, 0, 255};
      render_text_centered(sdl_context->renderer, font->fonts[OTHER], buffer, black,
                           sdl_context->win_center);

      player_ptr = starting_turn;
      do {
        // printf("%d\n", __LINE__);
        int id = player_ptr->id;
        SDL_Rect coin_rect = {
            .x = player_pos[id].x + card_area.w, .y = player_pos[id].y - card_area.h, 48, 48};
        SDL_RenderCopy(sdl_context->renderer, coin_texture, NULL, &coin_rect);
        char coins_text[24] = {0};
        snprintf(coins_text, sizeof coins_text, "%d", player_ptr->coins);
        SDL_Rect dest = {coin_rect.x + coin_rect.w * 1.2, coin_rect.y + coin_rect.h / 4, 0, 0};
        render_text_plain(sdl_context->renderer, font->fonts[OTHER], coins_text,
                          get_color(COLOR_BLACK), &dest);

        char name_text[sizeof(player_ptr->nick)] = {0};
        snprintf(name_text, sizeof name_text, "%s", player_ptr->nick);
        SDL_Rect dest_name = {player_pos[id].x + 30, player_pos[id].y + (card_area.h * 1.2), 40,
                              20};

        bool blink = id == turn->id && !game_state.winner_declared;
        render_nick(sdl_context->renderer, font->fonts[OTHER], name_text, get_color(COLOR_BLACK),
                    &dest_name, blink);

      } while ((player_ptr = get_next_connected_client(players_array, player_ptr->id)) !=
               starting_turn);
    }

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }
  SDL_DestroyTexture(coin_texture);

  // Mix_FreeChunk(card_sound);
  // Mix_CloseAudio();
}
