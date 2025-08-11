/*
 client.c
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

#include <deckhandler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"
#include "game.h"
#include "graphics.h"
#include "util.h"

const uint8_t MAX_CONNECTION_ATTEMPTS = 12;

#define POT_BOUNDARY SCALE_Y(250)

#define x_begin_action_button SCALE_X(500)

#define ma_sound_start_checked(pSound) ma_sound_start_wrap((pSound), __FILE__, __LINE__)

static int send_protocol_header(TCPsocket sock) {
  verbose_puts("Exchanging protocol information...");
  GameProtocolHeader_t hdr = {0};
  snprintf(hdr.magic, sizeof(hdr.magic), "%s", GAME_PROTOCOL_MAGIC);
  hdr.version = SDL_SwapBE16(GAME_PROTOCOL_VERSION);

  return send_all_tcp(sock, &hdr, sizeof(hdr));
}

// Build fails using gcc on Ubuntu 24.04 (and maybe others) without this
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// What's the max this needs to be to support the unicode suit symbol?
#define SIZEOF_CARD_TEXT 20

#define MAX_POT_COINS 80

SDL_Rect card_area = {0};

int8_t send_game_select(TCPsocket sock, uint8_t game_type, bool deuces_wild) {
  GameSelectPayload_t payload = {game_type, deuces_wild ? 1 : 0};

  const uint32_t payload_size = OPCODE_SIZE + sizeof(payload);
  const uint32_t total_size_be = SDL_SwapBE32(payload_size);

  uint8_t buffer[LENGTH_PREFIX_SIZE + payload_size];

  // Write length prefix
  memcpy(buffer, &total_size_be, LENGTH_PREFIX_SIZE);

  // Write opcode (portable big-endian)
  uint16_t opcode_be = SDL_SwapBE16(MSG_GAME_SELECT);
  memcpy(buffer + LENGTH_PREFIX_SIZE, &opcode_be, sizeof(opcode_be));

  // Write payload
  memcpy(buffer + LENGTH_PREFIX_SIZE + OPCODE_SIZE, &payload, sizeof(payload));

  // Send all
  int result = send_all_tcp(sock, buffer, sizeof(buffer));

  const GameChoice_t *choice = find_game_choice_by_type(game_type);
  if (result == 0) {
    verbose_printf("Game type sent: %s (Deuces wild: %s)\n", choice ? choice->str : "Unknown",
                   deuces_wild ? "Yes" : "No");
    return 0;
  }

  fprintf(stderr, "Game type failed to send: %s\n", choice ? choice->str : "Unknown");
  return result;
}

// These two buttons for creating the buttons are mostly identical. In the future,
// they can be changed so there are some differences if desired. Otherwise,
// they'll be merged, and some of the values, such as the colors, will be passed
// as arguments.
static Button_t create_button(const char *text, SDL_Renderer *renderer, const int y, TTF_Font *font,
                              SDL_Keycode key, const bool secondary) {
  Button_t button = {
      .text = text,
      .renderer = renderer,
      .bg_color = get_color(COLOR_BLACK),
      .fg_color = get_color(COLOR_YELLOW),
      .rect = (SDL_Rect){0, y, 0, 0},
      .font = font,
      .hovered = false,
      .enabled = true,
      .selected = false,
      .active = true,
      .hotkey = key,
  };

  // This should help avoid the button acidentally being clicked when someone double-clicks
  // on the previous action button
  if (secondary) {
    TTF_SizeUTF8(font, text, &button.rect.w, &button.rect.h);
    button.rect.y += button.rect.h + SCALE_Y(10);
  }

  if (TTF_SizeUTF8(font, text, &button.rect.w, &button.rect.h) != 0)
    fprintf(stderr, "TTF_SizeUTF8 error: %s\n", TTF_GetError());

  button.rect.w += SCALE_X(20);
  button.rect.h += SCALE_Y(10);
  return button;
}

static Button_t create_game_choice_button(const char *text, SDL_Renderer *renderer, SDL_Rect rect,
                                          TTF_Font *font, SDL_Keycode hotkey) {
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
      .active = true,
      .hotkey = hotkey,
  };
  return button;
}

void render_link(Link_t *link) {
  TTF_SetFontStyle(link->font, TTF_STYLE_UNDERLINE);

  SDL_Color text_color = (link->hovered) ? get_color(COLOR_BLUE) : get_color(COLOR_BLACK);

  SDL_Surface *surface = TTF_RenderText_Solid(link->font, link->text, text_color);
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
  TTF_SetFontStyle(link->font, TTF_STYLE_NORMAL);
}

static void ma_sound_start_wrap(ma_sound *pSound, const char *file, const int line) {
  ma_result result = ma_sound_start(pSound);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "[ma_sound_start] Failed (%s:%d) -> result = %d\n", file, line, result);
  }
}

static bool menu_display_game_choices(const PlayerConfig_t *player_config,
                                      SocketContext_t *socket_context, const int8_t my_id,
                                      GameState_t *game_state, ClientState_t *client_state,
                                      SdlContext_t *sdl_context, Font_t *font,
                                      const SoundContext_t *sound_context) {
  // This will likely get used later. For now, suppress the warning about "unused parameter"
  (void)player_config;

  uint8_t n_clients = 0;

  int y_offset = 160;
  Button_t game_choice_button[MAX_CHOICES];
  for (int i = 0; i < MAX_CHOICES; i++) {
    // TODO: Figure out alignment/justification
    SDL_Rect rect = {100, y_offset, 0, 0};
    if (TTF_SizeUTF8(font->fonts[FONT_BOLD], game_choices[i].str, &rect.w, &rect.h) != 0)
      fprintf(stderr, "TTF_SizeUTF8 error: %s\n", TTF_GetError());
    rect.w += 30;
    rect.h += rect.h * 0.1;
    game_choice_button[i] = create_game_choice_button(game_choices[i].str, sdl_context->renderer,
                                                      rect, font->fonts[FONT_BOLD], (SDL_Keycode)0);

    int button_height = rect.h + (rect.h * 0.2);
    y_offset += button_height;
  }

  bool running = true;

  const int link_column = sdl_context->win_center.x + card_area.h;
  Link_t link[] = {
      /* TRANSLATORS: "Discord", "Lazarus Project" should not be translated */
      {_(" Discord Channel (on Lazarus Project Server) "),
       "https://discord.com/channels/1295630985429516299/1385298664192217138",
       font->fonts[FONT_LINK], sdl_context->renderer, (SDL_Rect){link_column, 0, 0, 0}, false},
      {" Matrix ", "https://matrix.to/#/#dealers-choice:matrix.org", font->fonts[FONT_LINK],
       sdl_context->renderer, (SDL_Rect){link_column, 0, 0, 0}, false},
      {" Website ", DEALERSCHOICE_URL, font->fonts[FONT_LINK], sdl_context->renderer,
       (SDL_Rect){link_column, 0, 0, 0}, false}};

  for (size_t i = 0; i < ARRAY_SIZE(link); i++) {
    if (TTF_SizeUTF8(link[i].font, link[i].text, &link[i].rect.w, &link[i].rect.h) != 0)
      fprintf(stderr, "TTF_SizeUTF8 error: %s\n", TTF_GetError());
    link[i].rect.y = (sdl_context->window_height - (link[i].rect.h * 2)) - (i * link[i].rect.h) -
                     (i * (link[i].rect.h * 0.2));
  }

  Button_t deuces_wild = {
      _("Deuces Wild"),
      sdl_context->renderer,
      get_color(COLOR_WHITE),
      get_color(COLOR_BROWN),
      {0, 0, 0, 0},
      font->fonts[FONT_BOLD],
      false,
      true,
      false,
      true,
      0,
  };

  static uint8_t saved_n_clients = 0;
  TCPsocket sock = socket_context->sock;

  while (running && game_state->at_menu) {
    ERecvStatus_t recv_status = recv_game_state(socket_context, game_state, client_state, my_id);
    if (recv_status == RECV_ERROR)
      return false;

    for (int i = 0; i < MAX_CHOICES; i++)
      game_choice_button[i].enabled = (game_state->dealer_id == my_id && n_clients > 1);

    deuces_wild.enabled = (game_state->dealer_id == my_id && n_clients > 1);

    SDL_Point mouse_pos;
    SDL_GetMouseState(&mouse_pos.x, &mouse_pos.y);
    for (int i = 0; i < MAX_CHOICES; i++) {
      game_choice_button[i].hovered = SDL_PointInRect(&mouse_pos, &game_choice_button[i].rect);
    }

    deuces_wild.rect = (SDL_Rect){sdl_context->win_center.x, sdl_context->win_center.y / 2, 0, 0};
    TTF_SizeUTF8(deuces_wild.font, deuces_wild.text, &deuces_wild.rect.w, &deuces_wild.rect.h);
    deuces_wild.rect.w += SCALE_X(10);
    deuces_wild.rect.h += SCALE_Y(10);
    deuces_wild.hovered = SDL_PointInRect(&mouse_pos, &deuces_wild.rect) && deuces_wild.enabled;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      // SDL_Point mouse_pos = {e.button.x, e.button.y};
      for (size_t i = 0; i < ARRAY_SIZE(link); i++) {
        link[i].hovered = SDL_PointInRect(&mouse_pos, &link[i].rect);
      }

      if (e.type == SDL_QUIT) {
        return false;
      } else if (e.type == SDL_KEYDOWN &&
                 (e.key.keysym.sym == SDLK_RETURN && e.key.keysym.mod & KMOD_ALT)) {
        toggle_fullscreen(sdl_context->window);
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        for (int i = 0; i < MAX_CHOICES; i++) {
          if (SDL_PointInRect(&mouse_pos, &game_choice_button[i].rect) &&
              game_state->dealer_id == my_id) {
            if (send_game_select(sock, game_choices[i].game_type, deuces_wild.selected) == 0) {
              running = false;
              break;
            } else {
              return false;
            }
          }
        }
        for (size_t i = 0; i < sizeof link / sizeof link[0]; i++) {
          if (link[i].hovered && e.button.button == SDL_BUTTON_LEFT)
            if (SDL_OpenURL(link[i].url) == -1)
              fputs(SDL_GetError(), stderr);
        }
        if (deuces_wild.hovered)
          deuces_wild.selected = !deuces_wild.selected;
      }
    }

    clear_screen(sdl_context->renderer);

    for (int i = 0; i < MAX_CHOICES; i++)
      render_button(&game_choice_button[i]);

    render_button(&deuces_wild);

    SDL_Point status_pos = {
        sdl_context->window_width * .1,
        sdl_context->window_height / 2,
    };
    int offset_x = status_pos.x, offset_y = status_pos.y;

    SDL_Rect text_connected = {offset_x, offset_y, 0, 0};
    render_text_plain(sdl_context->renderer, font->fonts[FONT_BOLD], _("Connected players:"),
                      get_color(COLOR_BLACK), &text_connected);
    offset_x += SCALE_X(10);

    Player_t *client = &game_state->player[my_id];
    Player_t *start = client;

    n_clients = 0;
    do {
      int ping_column_x = sdl_context->win_center.x - SCALE_X(100); // fixed right edge for ping
      offset_y += SCALE_Y(40);

      // Build nickname + dealer label
      char nick_buf[SIZEOF_NICK + sizeof(" (Dealer)")];
      snprintf(nick_buf, sizeof nick_buf, "%s%s", client->nick,
               game_state->dealer_id == client->id ? " (Dealer)" : "");

      // Build ping text
      char ping_buf[32];
      snprintf(ping_buf, sizeof ping_buf, "ping %ums", client_state->ping_times[client->id]);

      // Render nick left-aligned
      SDL_Rect nick_pos = {offset_x, offset_y, 0, 0};
      render_text_plain(sdl_context->renderer, font->fonts[FONT_BOLD], nick_buf,
                        get_color(COLOR_WHITE), &nick_pos);

      // Measure ping text width
      int ping_w = 0, ping_h = 0;
      if (TTF_SizeUTF8(font->fonts[FONT_BOLD], ping_buf, &ping_w, &ping_h) != 0) {
        fprintf(stderr, "TTF_SizeUTF8 failed: %s\n", TTF_GetError());
        ping_w = 0;
      }

      // Render ping right-aligned at ping_column_x
      SDL_Rect ping_pos = {ping_column_x - ping_w, offset_y, 0, 0};
      render_text_plain(sdl_context->renderer, font->fonts[FONT_BOLD], ping_buf,
                        get_color(COLOR_WHITE), &ping_pos);

      n_clients++;
      client = get_next_connected_client(game_state->player, client->id);
    } while (client && client != start);
    if (saved_n_clients < n_clients && saved_n_clients != 0)
      ma_sound_start_checked(&sound_context->sounds[SND_SERVER_JOIN].sound);
    saved_n_clients = n_clients;

    if (n_clients == 1)
      render_text_plain(
          sdl_context->renderer, font->fonts[FONT_DEFAULT], "Waiting for more players...",
          get_color(COLOR_WHITE),
          &(SDL_Rect){sdl_context->win_center.x, sdl_context->window_height - 200, 0, 0});
    if (game_state->dealer_id != my_id)
      render_text_plain(
          sdl_context->renderer, font->fonts[FONT_DEFAULT], "Waiting for dealer to select game...",
          get_color(COLOR_WHITE),
          &(SDL_Rect){sdl_context->win_center.x, sdl_context->window_height - 200, 0, 0});

    for (size_t i = 0; i < ARRAY_SIZE(link); i++)
      render_link(&link[i]);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }
  return true;
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

int8_t send_player_action(ClientState_t *client_state, TCPsocket sock, uint8_t action,
                          uint32_t amount) {
  uint8_t buffer[7];

  buffer[0] = (MSG_PLAYER_ACTION >> 8) & 0xFF;
  buffer[1] = (MSG_PLAYER_ACTION) & 0xFF;
  buffer[2] = action;

  buffer[3] = (amount >> 24) & 0xFF;
  buffer[4] = (amount >> 16) & 0xFF;
  buffer[5] = (amount >> 8) & 0xFF;
  buffer[6] = (amount) & 0xFF;

  client_state->bet_check_fold = false;
  client_state->call_raise_fold = false;

  return send_all_tcp(sock, buffer, sizeof(buffer));
}

int8_t send_discards_request_new_cards(TCPsocket sock, const uint8_t *discard_indices,
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

typedef struct {
  char text[SIZEOF_CARD_TEXT];
  SDL_Color textColor;
  SDL_Renderer *renderer;
  // SDL_Color bg_color;
  // SDL_Color fg_color;
  SDL_Rect rect;
  bool hovered, selected, is_back, is_null;
  bool is_wild;
} CardContext_t;

static void render_card(CardContext_t *context, TTF_Font *font, const bool my_card,
                        const bool do_wild_exchange) {
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
  if (context->hovered && my_card) {
    SDL_SetRenderDrawBlendMode(context->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(context->renderer, 255, 255, 128, 96); // translucent yellow
    SDL_RenderFillRect(context->renderer, &context->rect);
    SDL_SetRenderDrawBlendMode(context->renderer, SDL_BLENDMODE_NONE);
  }

  if (context->selected)
    mark_selected(context->renderer, &context->rect);

  if (context->is_wild && do_wild_exchange) {
    Uint32 ticks = SDL_GetTicks();
    // Blink every 500 ms
    bool blink_on = (ticks / 500) % 2 == 0;

    if (blink_on) {
      SDL_SetRenderDrawBlendMode(context->renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(context->renderer, 255, 165, 0, 128); // translucent orange
      SDL_RenderFillRect(context->renderer, &context->rect);
      SDL_SetRenderDrawBlendMode(context->renderer, SDL_BLENDMODE_NONE);
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

  SDL_Rect textRect = {context->rect.x + (card_area.w - textSurface->w) / 2,
                       context->rect.y + (card_area.h - textSurface->h) / 2, textSurface->w,
                       textSurface->h};

  SDL_RenderCopy(context->renderer, textTexture, NULL, &textRect);
  SDL_FreeSurface(textSurface);
  SDL_DestroyTexture(textTexture);
}

static void make_human_readable_card(DH_Card *card, CardContext_t *context) {
  const char *face = DH_get_card_face_str(card->face_val);
  const char *suit = DH_get_card_unicode_suit(*card);
  context->textColor = (card->suit == DH_SUIT_HEARTS || card->suit == DH_SUIT_DIAMONDS)
                           ? get_color(COLOR_RED)
                           : get_color(COLOR_BLACK);
  snprintf(context->text, sizeof(context->text), "%s%s", face, suit);
  if (strlen(context->text) == 0) {
    fprintf(stderr, "%s:String length 0\n", __func__);
    exit(EXIT_FAILURE);
  }
}

static void create_card_context(CardContext_t card_context[MAX_PLAYERS][MAX_HAND_SIZE],
                                const int start_i, Player_t *players_array,
                                const SDL_Point *player_pos, SDL_Renderer *renderer,
                                const bool deuces_wild) {
  memset(card_context, 0, sizeof(CardContext_t) * MAX_PLAYERS * MAX_HAND_SIZE);
  Player_t *turn = &players_array[start_i];
  Player_t *starting_turn = turn;
  do {
    for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
      CardContext_t context = {
          .renderer = renderer,
          .hovered = false,
          .selected = false,
          .is_wild = false,
      };
      // printf("%d\n", __LINE__);
      const int id = turn->id;
      DH_Card *card = &(turn->hand.card)[card_n];
      const SDL_Point card_pos = {
          player_pos[id].x + card_n * (card_area.w + SCALE_X(10)),
          player_pos[id].y,
      };
      SDL_Rect rect = {card_pos.x, card_pos.y, card_area.w, card_area.h};
      context.rect = rect;

      SDL_Color textColor = {0, 0, 0, 0};
      // Initialize even though it's not used for backs and null cards
      context.textColor = textColor;

      context.is_null = DH_is_card_null(*card);
      if (!turn->in && !context.is_null)
        memcpy(card, &DH_card_back, sizeof(DH_card_back));

      context.is_back = DH_is_card_back(*card);

      if (!context.is_back && !context.is_null) {
        // Use a condition here so is_wild is not set to false if the card has
        // been changed
        if (!context.is_wild && deuces_wild)
          context.is_wild = card->face_val == DH_CARD_TWO;

        make_human_readable_card(card, &context);
      }
      card_context[id][card_n] = context;
    }
    turn = get_next_connected_client(players_array, turn->id);
  } while (turn && turn != starting_turn);
}

typedef struct {
  const char *front;
} Coin_t;

static SDL_Texture *load_coin_texture(SDL_Renderer *renderer, const char *base_path,
                                      const char *coin) {
  const char *subdir = "/images/";
  size_t len = strlen(base_path) + strlen(subdir) + strlen(coin) + 1;

  char *full_path = calloc_wrap(len, 1);
  snprintf(full_path, len, "%s%s%s", base_path, subdir, coin);

  SDL_Texture *tex = load_texture(renderer, full_path);
  free(full_path);
  return tex;
}

typedef struct {
  SDL_Texture *texture;
  SDL_Point start;
  SDL_Point end;
  uint32_t start_time;
  uint32_t duration; // in milliseconds
  bool active;
} CoinAnimation_t;

void render_coin_animation(SDL_Renderer *renderer, CoinAnimation_t *anim) {
  if (!anim->active) {
    SDL_RenderCopy(renderer, anim->texture, NULL,
                   &(SDL_Rect){anim->end.x, anim->end.y, SCALE_X(48), SCALE_Y(48)});
    return;
  }

  uint32_t now = SDL_GetTicks();
  float progress = (now - anim->start_time) / (float)anim->duration;

  if (progress >= 1.0f) {
    progress = 1.0f;
    anim->active = false; // Animation is done
  }

  int x = anim->start.x + (int)((anim->end.x - anim->start.x) * progress);
  int y = anim->start.y + (int)((anim->end.y - anim->start.y) * progress);

  SDL_Rect dst = {x, y, SCALE_X(48), SCALE_Y(48)};
  SDL_RenderCopy(renderer, anim->texture, NULL, &dst);
}

enum {
  CHECK,
  BET,
  FOLD,
  CALL,
  RAISE,
  DISCARD,
  EXCHANGE,
  MAX_ACTIONS,
};

static bool run_game_loop(const PlayerConfig_t *player_config, SocketContext_t *socket_context,
                          const GameSettings_t *game_settings, GameState_t *game_state,
                          SdlContext_t *sdl_context, const Font_t *font, Path_t *path,
                          const SoundContext_t *sound_context) {
  // This will likely get used later. For now, suppress the warning about "unused parameter"
  ClientState_t client_state = {0};
  card_area.w = SCALE_X(80);
  card_area.h = SCALE_Y(50);

  const SDL_Point player_pos[MAX_PLAYERS] = {
      // P0: bottom center
      {.x = SCALE_X(20), .y = card_area.h},

      // P1: left, 1/3 down
      {.x = SCALE_X(20), .y = card_area.h * 4},

      {.x = sdl_context->win_center.x, .y = card_area.h},

      {.x = sdl_context->win_center.x, .y = card_area.h * 4},

      {.x = sdl_context->win_center.x, .y = card_area.h * 7},
  };

  // This offers only a little extra protection if changes are made.
  _Static_assert(sizeof(player_pos) / sizeof(player_pos[0]) == 5,
                 "player_pos has wrong number of elements");

#define SIZEOF_STATUS_MSGS 16
  char status_msgs[SIZEOF_STATUS_MSGS][LEN_STATUS_STR] = {0};

  typedef struct {
    const char *text;
    SDL_Keycode key;
    bool secondary;
  } ActionButtonAttrs;

  ActionButtonAttrs action_button_attrs[MAX_ACTIONS] = {
      [CHECK] = {_("Check"), SDLK_c, false},      [BET] = {_("Bet"), SDLK_b, false},
      [FOLD] = {_("Fold"), SDLK_f, false},        [CALL] = {_("Call"), SDLK_c, false},
      [RAISE] = {_("Raise"), SDLK_r, false},      [DISCARD] = {_("Discard"), SDLK_d, true},
      [EXCHANGE] = {_("Exchange"), SDLK_x, true},
  };

  const int action_button_y = sdl_context->window_height - (card_area.h * 4);
  Button_t action_button[MAX_ACTIONS];
  for (int i = 0; i < MAX_ACTIONS; i++) {
    action_button[i] = create_button(action_button_attrs[i].text, sdl_context->renderer,
                                     action_button_y, font->fonts[FONT_BOLD],
                                     action_button_attrs[i].key, action_button_attrs[i].secondary);
  }

  const struct Amount_t {
    const uint32_t value;
    const SDL_Keycode hotkey;
  } amount[] = {{game_settings->bet_minimum, SDLK_1},
                {game_settings->bet_median, SDLK_2},
                {game_settings->bet_maximum, SDLK_3}};

  const size_t n_bet_amounts = ARRAY_SIZE(amount);
  Button_t amount_button[n_bet_amounts];

  char amount_str[n_bet_amounts][16];    // enough for uint32_t
  int current_x = x_begin_action_button; // Start position for first button

  for (size_t i = 0; i < n_bet_amounts; i++) {
    snprintf(amount_str[i], sizeof(amount_str[i]), "%" PRIu32, amount[i].value);

    int text_w = 0, text_h = 0;
    if (TTF_SizeText(font->fonts[FONT_BOLD], amount_str[i], &text_w, &text_h) != 0) {
      fprintf(stderr, "TTF_SizeText failed: %s\n", TTF_GetError());
      text_w = SCALE_X(60); // fallback width
    }

    int button_w = text_w + SCALE_X(20);
    int button_h = text_h + SCALE_Y(10);

    amount_button[i] = (Button_t){
        amount_str[i],
        sdl_context->renderer,
        get_color(COLOR_WHITE),
        get_color(COLOR_BROWN),
        (SDL_Rect){current_x, action_button_y + card_area.w, button_w, button_h},
        font->fonts[FONT_BOLD],
        false,
        true,
        false,
        true,
        amount[i].hotkey,
    };

    current_x += button_w + SCALE_X(10); // Move right for next button with spacing
  }

  amount_button[0].selected = true;

  CardContext_t card_context[MAX_PLAYERS][MAX_HAND_SIZE];

  Button_t card_faces[13] = {0};
  for (size_t i = 0; i < ARRAY_SIZE(card_faces); i++) {
    int card_val = i + 1;
    if (card_val == DH_CARD_TWO)
      continue;

    card_faces[i] = (Button_t){
        "",
        sdl_context->renderer,
        get_color(COLOR_WHITE),
        get_color(COLOR_BROWN),
        {0, 0, 0, 0},
        font->fonts[FONT_WILD_SELECT],
        false,
        true,
        false,
        true,
        0,
    };
  }

  Button_t card_suits[DH_SUIT_MAX] = {0};
  for (DH_suit i = 0; i < ARRAY_SIZE(card_suits); i++) {
    card_suits[i] = (Button_t){
        "",
        sdl_context->renderer,
        get_color(COLOR_WHITE),
        get_color(COLOR_BROWN),
        {0, 0, 0, 0},
        font->fonts[FONT_CARD],
        false,
        true,
        false,
        true,
        0,
    };
  }

  int running = 1;
  bool cards_created = false;

  Player_t *players_array = game_state->player;
  Player_t *turn = NULL;
  Player_t *starting_turn = NULL;

  Coin_t coin[] = {
      {
          "48x48_front_1907_Saint_Gaudens_gold_coin.png",
      },
      {"48x48_Hammurabi.png"},
      {
          "48x48_front_Gaius-Julius-Caesar-denarius-44-BC-RRC-480-3.png",
      },
      {
          "48x48-1984_rv_marie_curie.png",
      },
  };

  const int which_coin = pcg32_boundedrand_r(&rng, ARRAY_SIZE(coin));
  SDL_Texture *coin_tex_front =
      load_coin_texture(sdl_context->renderer, path->data, coin[which_coin].front);

  typedef struct {
    SDL_Point pt[MAX_POT_COINS];
  } PotCoin_t;

  PotCoin_t pot_coin = {0};
  uint8_t coins = 0;
  CoinAnimation_t coin_anim = {0};

  client_state.timer_start = SDL_GetTicks();

  client_state.selected_amount = amount[0].value;

  const int8_t my_id = game_settings->client_id;
  TCPsocket sock = socket_context->sock;

  while (running) {
    ERecvStatus_t recv_status = recv_game_state(socket_context, game_state, &client_state, my_id);
    // printf("%d\n", __LINE__);
    if (recv_status == RECV_ERROR)
      running = false;
    else if (recv_status == RECV_SUCCESS)
      cards_created = false;

    if (game_state->at_menu)
      break;

    int8_t *turn_id = &client_state.turn_id;

    if (!starting_turn)
      starting_turn = &game_state->player[*turn_id];

    // For cases when the client who was designated as starting_turn disconnects
    if (!starting_turn->is_connected) {
      starting_turn = get_next_player(players_array, starting_turn->id);
      if (!turn) {
        running = false;
        break;
      }
    }

    turn = &game_state->player[*turn_id];
    // printf("turn id: %d\n", turn->id);

    if (strcmp(client_state.server_status_str, status_msgs[SIZEOF_STATUS_MSGS - 1]) != 0) {
      // Shift messages up by one slot: [1]..[7] → [0]..[6]
      memmove(status_msgs, status_msgs + 1, sizeof(status_msgs[0]) * (SIZEOF_STATUS_MSGS - 1));

      // Copy new message to the bottom
      snprintf(status_msgs[SIZEOF_STATUS_MSGS - 1], sizeof(client_state.server_status_str), "%s",
               client_state.server_status_str);
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
                          sdl_context->renderer, game_state->deuces_wild);
      cards_created = true;
    }
    // printf("%d\n", __LINE__);

    clear_screen(sdl_context->renderer);

    bool new_coin = false;
    SDL_Point pot_pos = {player_pos[4].x - POT_BOUNDARY + card_area.h,
                         sdl_context->win_center.y + card_area.h};
    if (game_state->pot > coins * game_settings->bet_minimum && coins < MAX_POT_COINS) {
      pot_coin.pt[coins].x = pot_pos.x + pcg32_boundedrand_r(&rng, POT_BOUNDARY) - SCALE_X(150);
      pot_coin.pt[coins].y = pot_pos.y + pcg32_boundedrand_r(&rng, POT_BOUNDARY) - SCALE_Y(150);
      coins++;
      new_coin = true;
    } else if (game_state->pot == 0)
      coins = 0;

    int p;
    for (p = 0; p < coins; p++) {
      SDL_Rect coin_rect = {
          .x = pot_coin.pt[p].x,
          .y = pot_coin.pt[p].y,
          .w = SCALE_X(48),
          .h = SCALE_Y(48),
      };
      if (p < coins - 1)
        SDL_RenderCopy(sdl_context->renderer, coin_tex_front, NULL, &coin_rect);
    }

    p--;
    if (new_coin) {
      coin_anim = (CoinAnimation_t){
          .texture = coin_tex_front,
          .start = (SDL_Point){player_pos[turn->id].x, player_pos[turn->id].y},
          .end = (SDL_Point){pot_coin.pt[p].x, pot_coin.pt[p].y},
          .start_time = SDL_GetTicks(),
          .duration = 300,
          .active = true,
      };
      new_coin = false;
    }

    if (game_state->pot > 0)
      render_coin_animation(sdl_context->renderer, &coin_anim);

    for (int i = 0; i < SIZEOF_STATUS_MSGS; i++) {
      char tmp[sizeof(status_msgs[0])];
      snprintf(tmp, sizeof tmp, "%s", status_msgs[i]);
      SDL_Rect text_pos = {SCALE_X(40),
                           (sdl_context->win_center.y) + (SCALE_Y(20) * i) + SCALE_Y(5), 0, 0};
      render_text_plain(sdl_context->renderer, font->fonts[FONT_STATUS_MSG], tmp,
                        get_color(COLOR_BLACK), &text_pos);
    }

    Player_t *player_ptr = starting_turn;
    for (int card_n = 0; card_n < MAX_HAND_SIZE; ++card_n) {
      do {
        // printf("%d\n", __LINE__);
        render_card(&card_context[player_ptr->id][card_n], font->fonts[FONT_CARD],
                    player_ptr->id == my_id, client_state.do_exchange_wilds);

        player_ptr = get_next_connected_client(players_array, player_ptr->id);
      } while (player_ptr != starting_turn);
    }

    if (client_state.play_coin_sound) {
      ma_sound_start(
          &sound_context->coin_hit_sounds[pcg32_boundedrand_r(&rng, sound_context->coin_array_size)]
               .sound);
      client_state.play_coin_sound = false;
    }

    bool my_turn = *turn_id == my_id;
    if (client_state.turn_switch || game_state->winner_declared) {
      if (!client_state.end_game_timer_set) {
        client_state.timer_start = SDL_GetTicks();

        if (game_state->winner_declared)
          client_state.end_game_timer_set = true;

        // Handle timeout: If there was no action by the player, one of these
        // may be set to true
        client_state.bet_check_fold = false;
        client_state.call_raise_fold = false;
        client_state.do_discard_draw = false;
        client_state.do_exchange_wilds = false;

        if (my_turn && !game_state->winner_declared) {
          if (player_config->turn_notify)
            ma_sound_start_checked(&sound_context->sounds[SND_MY_TURN].sound);
        }
        client_state.turn_switch = false;
      }
    }

    uint32_t now = SDL_GetTicks();
    int32_t remaining_ms;
    if (game_state->winner_declared) {
      remaining_ms = (int32_t)(client_state.timer_start + game_settings->end_of_game_timeout_ms) -
                     (int32_t)now;
    } else {

      remaining_ms =
          (int32_t)(client_state.timer_start + game_settings->action_timeout_ms) - (int32_t)now;

      if (client_state.do_discard_draw) {
        for (int i = 0; i < MAX_HAND_SIZE; i++)
          if (turn->hand.card[i].face_val == DH_CARD_ACE) {
            client_state.has_ace = true;
            break;
          }
        uint8_t max_allowed = client_state.has_ace ? 4 : 3;
        action_button[DISCARD].enabled = client_state.n_cards_selected <= max_allowed;
        action_button[DISCARD].rect.x = x_begin_action_button;
        if (!action_button[DISCARD].enabled) {
          char tmp[50] = {0};
          snprintf(tmp, sizeof(tmp), "You may only discard a maximum of %d cards", max_allowed);
          render_text_plain(sdl_context->renderer, font->fonts[FONT_BOLD], tmp,
                            get_color(COLOR_WHITE),
                            &(SDL_Rect){action_button[DISCARD].rect.x,
                                        action_button[DISCARD].rect.y + card_area.h, 0, 0});
        }
        render_button(&action_button[DISCARD]);
      } else if (client_state.do_exchange_wilds) {
        action_button[EXCHANGE].enabled = true;
        action_button[EXCHANGE].rect.x = x_begin_action_button;
        if (action_button[EXCHANGE].enabled) {
          const char *tmp = _("Click a 2, then choose value and suit to assign.");
          render_text_plain(sdl_context->renderer, font->fonts[FONT_BOLD], tmp,
                            get_color(COLOR_WHITE),
                            &(SDL_Rect){action_button[EXCHANGE].rect.x,
                                        action_button[EXCHANGE].rect.y + card_area.h, 0, 0});
        }
        render_button(&action_button[EXCHANGE]);
      } else if (client_state.bet_check_fold || client_state.call_raise_fold) {
        int x_offset = x_begin_action_button;
        if (client_state.bet_check_fold) {
          action_button[BET].rect.x = x_offset;
          render_button(&action_button[BET]);
          x_offset = action_button[BET].rect.x + action_button[BET].rect.w + BUTTON_X_SPACING;

          action_button[CHECK].rect.x = x_offset;
          render_button(&action_button[CHECK]);
          x_offset = action_button[CHECK].rect.x + action_button[CHECK].rect.w + BUTTON_X_SPACING;
        } else if (client_state.call_raise_fold) {
          action_button[CALL].rect.x = x_offset;
          render_button(&action_button[CALL]);
          x_offset = action_button[CALL].rect.x + action_button[CALL].rect.w + BUTTON_X_SPACING;

          action_button[RAISE].rect.x = x_offset;
          action_button[RAISE].active = game_state->raises_remaining > 0;
          render_button(&action_button[RAISE]);
          x_offset = action_button[RAISE].rect.x + action_button[RAISE].rect.w + BUTTON_X_SPACING;
        }
        action_button[FOLD].rect.x = x_offset;
        render_button(&action_button[FOLD]);

        // The amount buttons won't be shown if this is true (max raises were reached
        // if the RAISE button is not active).
        if (client_state.bet_check_fold ||
            (client_state.call_raise_fold && action_button[RAISE].active)) {
          for (size_t i = 0; i < n_bet_amounts; i++)
            render_button(&amount_button[i]);
        }
      }
    }

    if (remaining_ms > 0) {
      int elapsed = remaining_ms / 1000;
      char elapsed_str[8] = {0};
      snprintf(elapsed_str, sizeof(elapsed_str), "%d", elapsed);

      render_text_plain(
          sdl_context->renderer, font->fonts[FONT_BOLD], elapsed_str, get_color(COLOR_WHITE),
          &(SDL_Rect){sdl_context->window_width - 60, sdl_context->window_height - 60, 0, 0});
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%" PRIu32, game_state->pot);
    SDL_Color black = {0, 0, 0, 255};
    render_text_centered(sdl_context->renderer, font->fonts[FONT_DEFAULT_BOLD], buffer, black,
                         pot_pos);

    player_ptr = starting_turn;
    do {
      // printf("%d\n", __LINE__);
      int id = player_ptr->id;
      char name_text[sizeof(player_ptr->nick)] = {0};
      snprintf(name_text, sizeof name_text, "%s", player_ptr->nick);
      SDL_Rect name_rect = {player_pos[id].x + card_area.w / 2,
                            player_pos[id].y + (card_area.h * 1.2), 0, 0};

      bool blink = id == turn->id && !game_state->winner_declared;
      render_nick(sdl_context->renderer, font->fonts[FONT_BOLD], name_text, get_color(COLOR_BLACK),
                  &name_rect, blink);

      if (TTF_SizeUTF8(font->fonts[FONT_BOLD], name_text, &name_rect.w, &name_rect.h) != 0)
        fprintf(stderr, "TTF_SizeUTF8 error: %s\n", TTF_GetError());
      SDL_Rect coin_rect = {
          .x = name_rect.x + name_rect.w + 10, .y = name_rect.y, SCALE_X(48), SCALE_Y(48)};
      SDL_RenderCopy(sdl_context->renderer, coin_tex_front, NULL, &coin_rect);
      char coins_text[24] = {0};
      snprintf(coins_text, sizeof coins_text, "%" PRId32, player_ptr->coins);
      SDL_Rect coin_text_rect = {coin_rect.x + coin_rect.w * 1.2, name_rect.y, 0, 0};
      render_text_plain(sdl_context->renderer, font->fonts[FONT_BOLD], coins_text,
                        get_color(COLOR_BLACK), &coin_text_rect);

      player_ptr = get_next_connected_client(players_array, player_ptr->id);
    } while (player_ptr && player_ptr != starting_turn);

    if (game_state->deuces_wild && client_state.do_exchange_wilds) {
      int y_offset = card_area.h * 1;
      int width, height;
      TTF_SizeUTF8(font->fonts[FONT_WILD_SELECT], " 10 ", &width, &height);
      for (size_t i = 0; i < ARRAY_SIZE(card_faces); i++) {
        int card_val = i + 1;
        if (card_val == DH_CARD_TWO)
          continue;
        card_faces[i].text = DH_get_card_face_str(card_val);
        card_faces[i].rect =
            (SDL_Rect){sdl_context->win_center.x - SCALE_X(100), y_offset, width, height};
        y_offset += height + SCALE_Y(10);
        render_button(&card_faces[i]);
      }

      y_offset = (height + SCALE_Y(10)) * 6;
      TTF_SizeUTF8(font->fonts[FONT_CARD], "   ", &width, &height);
      for (DH_suit i = 0; i < ARRAY_SIZE(card_suits); i++) {
        card_suits[i].text = DH_get_unicode_suit(i);
        card_suits[i].rect =
            (SDL_Rect){sdl_context->win_center.x - SCALE_X(100) + width + SCALE_X(20), y_offset,
                       width, height};
        y_offset += height + SCALE_Y(10);
        render_button(&card_suits[i]);
      }
    }

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      SDL_Point mouse_pos = {event.button.x, event.button.y};
      for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
        DH_Card *card = &turn->hand.card[card_n];
        if (!DH_is_card_null(*card) || !DH_is_card_null(*card)) {
          card_context[my_id][card_n].hovered =
              SDL_PointInRect(&mouse_pos, &card_context[my_id][card_n].rect);
          if (card_context[my_id][card_n].hovered && event.type == SDL_MOUSEBUTTONDOWN &&
              client_state.do_discard_draw) {
            // select or deselect when clicked
            bool *selected = &card_context[my_id][card_n].selected;
            *selected = !(*selected);

            // Update counter
            if (*selected) {
              client_state.n_cards_selected++;
            } else {
              client_state.n_cards_selected--;
            }
            // printf("n_selected: %d\n", client_state.n_cards_selected);
          }
          // If the mouse is at the location, there's no need to iterate through the rest
          // of the cards.
          if (card_context[my_id][card_n].hovered)
            break;
        }
      }
      if (client_state.do_exchange_wilds) {
        int wild_card_selected = -1;
        for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
          card_context[my_id][card_n].hovered =
              SDL_PointInRect(&mouse_pos, &card_context[my_id][card_n].rect);
          if (card_context[my_id][card_n].is_wild && card_context[my_id][card_n].hovered &&
              event.type == SDL_MOUSEBUTTONDOWN)
            wild_card_selected = card_n;
          if (wild_card_selected != -1) {
            if (!card_context[my_id][card_n].selected) {
              for (size_t j = 0; j < MAX_HAND_SIZE; j++) {
                card_context[my_id][j].selected = false;
              }
              card_context[my_id][card_n].selected = true;
            }
            break;
          }
        }
        bool wild_changed = false;
        for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
          if (card_context[my_id][card_n].selected) {
            for (DH_card_face f = 0; f < (DH_card_face)ARRAY_SIZE(card_faces); f++) {
              DH_card_face card_val = f + 1;
              if (card_val == DH_CARD_TWO)
                continue;
              if (SDL_PointInRect(&mouse_pos, &card_faces[f].rect) &&
                  event.type == SDL_MOUSEBUTTONDOWN) {
                DH_Card *card = &turn->hand.card[card_n];
                card->face_val = card_val;
                make_human_readable_card(card, &card_context[my_id][card_n]);
                break;
              }
            }
            for (DH_suit s = DH_SUIT_MAX - DH_SUIT_MAX; s < ARRAY_SIZE(card_suits); s++) {
              if (SDL_PointInRect(&mouse_pos, &card_suits[s].rect) &&
                  event.type == SDL_MOUSEBUTTONDOWN) {
                DH_Card *card = &turn->hand.card[card_n];
                card->suit = s;
                make_human_readable_card(card, &card_context[my_id][card_n]);
                break;
              }
            }
          }
        }
        if (wild_changed)
          break; // from sdl event loop only
      }

      for (int i = 0; i < MAX_ACTIONS; i++) {
        action_button[i].hovered = SDL_PointInRect(&mouse_pos, &action_button[i].rect);
      }
      bool amount_selected = false;
      for (size_t i = 0; i < n_bet_amounts; i++) {
        amount_button[i].hovered = SDL_PointInRect(&mouse_pos, &amount_button[i].rect);
        amount_selected =
            ((amount_button[i].hovered && event.type == SDL_MOUSEBUTTONDOWN) ||
             (event.type == SDL_KEYDOWN && event.key.keysym.sym == amount_button[i].hotkey));
        if (amount_selected) {
          if (!amount_button[i].selected) {
            for (size_t j = 0; j < n_bet_amounts; j++) {
              amount_button[j].selected = false;
            }
            amount_button[i].selected = true;
            client_state.selected_amount = atoi(amount_button[i].text);
          }
          break;
        }
      }
      if (amount_selected)
        break;
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN &&
                 (event.key.keysym.sym == SDLK_RETURN && event.key.keysym.mod & KMOD_ALT)) {
        toggle_fullscreen(sdl_context->window);
      } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN) {
        if (my_turn && !client_state.do_discard_draw && !client_state.do_exchange_wilds) {
          if (client_state.bet_check_fold || client_state.call_raise_fold) {
            if (SDL_PointInRect(&mouse_pos, &action_button[FOLD].rect) ||
                event.key.keysym.sym == action_button[FOLD].hotkey) {
              verbose_puts("folding");
              if (send_player_action(&client_state, sock, ACTION_FOLD, 0) != 0)
                fprintf(stderr, "Failed to fold\n");
            }
          }
          if (client_state.bet_check_fold) {
            // TODO: use existing array (or modify it) to loop through each action
            if (SDL_PointInRect(&mouse_pos, &action_button[BET].rect) ||
                event.key.keysym.sym == action_button[BET].hotkey) {
              verbose_puts("sending bet");
              if (send_player_action(&client_state, sock, ACTION_BET,
                                     client_state.selected_amount) != 0)
                fprintf(stderr, "Failed to send bet\n");
            } else if (SDL_PointInRect(&mouse_pos, &action_button[CHECK].rect) ||
                       event.key.keysym.sym == action_button[CHECK].hotkey) {
              verbose_puts("checking");
              if (send_player_action(&client_state, sock, ACTION_CHECK, 0) != 0)
                fprintf(stderr, "Failed to check\n");
            }
          } else if (client_state.call_raise_fold) {
            if (action_button[RAISE].active &&
                (SDL_PointInRect(&mouse_pos, &action_button[RAISE].rect) ||
                 event.key.keysym.sym == action_button[RAISE].hotkey)) {
              if (send_player_action(&client_state, sock, ACTION_RAISE,
                                     client_state.selected_amount) == 0)
                verbose_puts("raising");
              else
                fputs("Failed to raise\n", stderr);
            } else if (SDL_PointInRect(&mouse_pos, &action_button[CALL].rect) ||
                       event.key.keysym.sym == action_button[CALL].hotkey) {
              verbose_puts("calling");
              if (send_player_action(&client_state, sock, ACTION_CALL, 0) != 0)
                fprintf(stderr, "Failed to call\n");
            }
          }
        } else if (action_button[DISCARD].enabled && client_state.do_discard_draw &&
                   (SDL_PointInRect(&mouse_pos, &action_button[DISCARD].rect) ||
                    event.key.keysym.sym == action_button[DISCARD].hotkey)) {

          // Although the maximum allowed discards for 5 card draw can never
          // exceed 4, we need an array size of MAX_HAND_SIZE in case they select
          // all 5. However, the player will be required to have < 5 selected
          // to actually perform the discard.
          uint8_t discard_indices[MAX_HAND_SIZE] = {0};
          uint8_t discard_count = 0;

          for (int i = 0; i < MAX_HAND_SIZE; i++) {
            if (!card_context[my_id][i].selected)
              continue;
            discard_indices[discard_count++] = i;
          }

          // Reset the flag that's used to indicate to the client it's their turn to draw
          client_state.do_discard_draw = false;

          if (send_discards_request_new_cards(sock, discard_indices, discard_count) != 0)
            fprintf(stderr, "Failed to send discards\n");
          else {
            verbose_puts("Discards sent");
          }
        } else if (action_button[EXCHANGE].enabled &&
                   (SDL_PointInRect(&mouse_pos, &action_button[EXCHANGE].rect) ||
                    event.key.keysym.sym == action_button[EXCHANGE].hotkey)) {
          verbose_puts("exchanging wilds");
          POKEVAL_Hand_7 hand = {0};

          for (int i = 0; i < MAX_HAND_SIZE; i++) {
            if (!card_context[my_id][i].is_wild) {
              hand.card[i] = DH_card_null;
              continue;
            }
            hand.card[i] = turn->hand.card[i];
          }

          // Reset the flag that's used to indicate to the client it's their turn to draw
          client_state.do_exchange_wilds = false;

          size_t size = 0;
          uint8_t *data = serialize_hand(hand, &size);
          if (!data) {
            fprintf(stderr, "Failed to serialize hand\n");
            return -1;
          }

          // Just send the serialized protobuf
          if (send_all_tcp(sock, data, size) != 0) {
            fprintf(stderr, "Failed to send hand\n");
            free(data);
            return -1;
          } else
            verbose_puts("Wilds sent");
          free(data);
        }
      }
    } // End Poll event
  }
  SDL_DestroyTexture(coin_tex_front);
  return running;
}

SocketContext_t get_socket_context_and_run_client(PlayerConfig_t *player_config,
                                                  const CliArgs_t *cli_args, const char *host_str,
                                                  const uint16_t port, SdlContext_t *sdl_context,
                                                  Font_t *font, Path_t *path,
                                                  const bool test_mode) {
  IPaddress server_ip;
  SocketContext_t socket_context = {0};

  if (SDLNet_ResolveHost(&server_ip, host_str, port) == -1) {
    fprintf(stderr, "Failed to resolve server: %s\n", SDLNet_GetError());
    return socket_context;
  }

  uint8_t attempts;
  for (attempts = 0; attempts < MAX_CONNECTION_ATTEMPTS; ++attempts) {
    socket_context.sock = SDLNet_TCP_Open(&server_ip);
    if (socket_context.sock) {
      break; // Success
    }

    fprintf(stderr, "Attempt %d: Failed to connect to server: %s\n", attempts + 1,
            SDLNet_GetError());

    bool quit = false;
    if (attempts < MAX_CONNECTION_ATTEMPTS - 1) {
      Uint32 start = SDL_GetTicks();
      while (SDL_GetTicks() - start < 5000) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          if (e.type == SDL_QUIT)
            quit = true;
        }
        SDL_Delay(16);
        if (quit)
          break;
      }
    }
    if (quit) {
      attempts++;
      break;
    }
  }

  if (!socket_context.sock) {
    printf("All %d attempts failed. Giving up.\n", attempts);
    return socket_context;
  }

  TCPsocket sock = socket_context.sock;
  socket_context.set = SDLNet_AllocSocketSet(1);
  if (!socket_context.set) {
    fprintf(stderr, "Failed to allocate socket set: %s\n", SDLNet_GetError());
    SDLNet_TCP_Close(sock);
    return socket_context;
  }

  if (SDLNet_TCP_AddSocket(socket_context.set, sock) == -1)
    fputs("Socket set full\n", stderr);

  if (send_protocol_header(sock) != 0) {
    fputs("Failed to send protocol\n", stderr);
    goto cleanup;
  }

  if (!test_mode) {
    GameState_t game_state = {0};
    GameSettings_t game_settings = {0};
    ClientState_t client_state = {0};
    char *nick = player_config->nick;
    uint16_t len = strlen(nick) + 1;
    uint16_t net_len = SDL_SwapBE16(len);
    send_all_tcp(sock, &net_len, sizeof(net_len));
    if (send_all_tcp(sock, player_config->nick, len) != 0)
      fprintf(stderr, "Failed to send player nick to server\n");

    const Uint32 timeout = 2000;    // 2 seconds
    const Uint32 retry_delay = 100; // milliseconds per retry

    Uint32 start_time = SDL_GetTicks(); // milliseconds
    ERecvStatus_t recv_status;
    do {
      recv_status = recv_game_settings(sock, socket_context.set, &game_settings);
      if (recv_status == RECV_SUCCESS) {
        break;
      } else if (recv_status == RECV_ERROR) {
        fprintf(stderr, "Failed to receive game settings\n");
        exit(EXIT_FAILURE);
      }

      SDL_Delay(retry_delay);
    } while (SDL_GetTicks() - start_time < timeout);

    start_time = SDL_GetTicks(); // milliseconds
    do {
      recv_status =
          recv_game_state(&socket_context, &game_state, &client_state, game_settings.client_id);

      if (recv_status == RECV_SUCCESS) {
        break;
      } else if (recv_status == RECV_ERROR) {
        fprintf(stderr, "Failed to receive initial game state\n");
        exit(EXIT_FAILURE);
      }

      SDL_Delay(retry_delay);
    } while (SDL_GetTicks() - start_time < timeout);
    if (recv_status != RECV_SUCCESS)
      exit(EXIT_FAILURE);

    SoundContext_t sound_context = {0};
    if (player_config->volume == 0 || cli_args->disable_audio) {
      ma_engine_config_init();
      sound_context.engineConfig.noDevice = MA_TRUE;
      sound_context.engineConfig.channels = 2;       // Must be set when not using a device.
      sound_context.engineConfig.sampleRate = 48000; // Must be set when not using a device.
    } else // Obviously the engine gets initialized unconditionally, but I don't see
      // any reason to show this and confuse a user who has their volume set to 0
      verbose_puts("Initializing audio engine (powered by miniaudio: https://miniaud.io/)");

    sound_context.result = ma_engine_init(&sound_context.engineConfig, &sound_context.engine);
    if (sound_context.result != MA_SUCCESS) {
      fprintf(stderr, "Error: Failed to initialize miniaudio engine (code: %d).\n",
              sound_context.result);
      exit(EXIT_FAILURE);
    }

    ma_engine_set_volume(&sound_context.engine, player_config->volume * .1f);

    // Using {0} or {{0}} for the The ma_sound field initializer doesn't work so
    // using 'ma_tmp' instead
    ma_sound ma_tmp = {0};
    Sound_t sounds[] = {[SND_SERVER_JOIN] = {"server_join.wav", ma_tmp},
                        [SND_CARD_DEALT] = {"card_dealt.wav", ma_tmp},
                        [SND_MY_TURN] = {"my_turn.wav", ma_tmp}};

    Sound_t coin_hit_sounds[] = {
        {"coin_hit_001.wav", ma_tmp}, {"coin_hit_002.wav", ma_tmp}, {"coin_hit_003.wav", ma_tmp},
        {"coin_hit_004.wav", ma_tmp}, {"coin_hit_005.wav", ma_tmp}, {"coin_hit_006.wav", ma_tmp},
        {"coin_hit_007.wav", ma_tmp},
    };

    sound_context.coin_array_size = ARRAY_SIZE(coin_hit_sounds);

    sound_context.sounds = sounds;
    sound_context.coin_hit_sounds = coin_hit_sounds;

    PathconfLimits_t limits;
    get_pathconf_limits(path->data, &limits);
    size_t i;
    for (i = 0; i < SND_NUM_SOUNDS; i++) {
      char *snd_path = join_paths(limits.path_max, path->data, "sounds", sounds[i].filename);
      if (ma_sound_init_from_file(&sound_context.engine, snd_path, 0, NULL, NULL,
                                  &sounds[i].sound) != MA_SUCCESS) {
        fprintf(stderr, "Failed to init sound %zd\n", i);
        exit(EXIT_FAILURE);
      }
      free(snd_path);
    }

    for (i = 0; i < ARRAY_SIZE(coin_hit_sounds); i++) {
      char *snd_path =
          join_paths(limits.path_max, path->data, "sounds/coin", coin_hit_sounds[i].filename);
      if (ma_sound_init_from_file(&sound_context.engine, snd_path, 0, NULL, NULL,
                                  &coin_hit_sounds[i].sound) != MA_SUCCESS) {
        fprintf(stderr, "Failed to init sound %zd\n", i);
        exit(EXIT_FAILURE);
      }
      free(snd_path);
    }

    bool running = true;
    do {
      running =
          menu_display_game_choices(player_config, &socket_context, game_settings.client_id,
                                    &game_state, &client_state, sdl_context, font, &sound_context);
      if (!running)
        break;

      running = run_game_loop(player_config, &socket_context, &game_settings, &game_state,
                              sdl_context, font, path, &sound_context);
    } while (running);
    for (i = 0; i < SND_NUM_SOUNDS; i++)
      ma_sound_uninit(&sounds[i].sound);
    for (i = 0; i < ARRAY_SIZE(coin_hit_sounds); i++)
      ma_sound_uninit(&coin_hit_sounds[i].sound);
    ma_engine_uninit(&sound_context.engine);
  } else
    return socket_context;

cleanup:
  socket_cleanup(&socket_context);
  SDLNet_Quit();
  return socket_context;
}

void do_sdl_cleanup(SdlContext_t *sdl_context) {
  SDL_DestroyRenderer(sdl_context->renderer);
  SDL_DestroyWindow(sdl_context->window);
  SDL_Quit();
}
