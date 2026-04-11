/*
 client.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025,2026 Andy Alt

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
#include <stdatomic.h>

#include <deckhandler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"
#include "game.h"
#include "globals.h"
#include "graphics.h"
#include "widgets/button.h"
#include "widgets/dealer.h"
#include "widgets/image.h"
#include "widgets/indicator.h"
#include "widgets/nick.h"
#include "widgets/ping.h"
#include "widgets/text.h"

#include "util.h"

#ifdef HAVE_LIBSODIUM
#include <sodium.h>
#endif

static const uint8_t coin_px = 96;

#define POT_BOUNDARY 450

#define x_begin_action_button 500

#define ma_sound_start_checked(pSound) ma_sound_start_wrap((pSound), __FILE__, __LINE__)

// Build fails using gcc on Ubuntu 24.04 (and maybe others) without this
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// What's the max this needs to be to support the unicode suit symbol?
// SIZEOF_CARD_TEXT is defined in client.h

#define MAX_POT_COINS 40

static int send_protocol_header(TCPsocket sock) {
  verbose_puts("Exchanging protocol information...");
  GameProtocolHeader_t hdr = {0};
  snprintf(hdr.magic, sizeof(hdr.magic), "%s", GAME_PROTOCOL_MAGIC);
  hdr.version = SDL_SwapBE16(GAME_PROTOCOL_VERSION);

  if (send_all_tcp(sock, &hdr, sizeof(hdr)) != 0)
    return -1;

  uint8_t response;
  if (recv_all_tcp(sock, &response, sizeof(response)) <= 0) {
    fprintf(stderr, "Protocol version mismatch or server closed connection\n");
    return -1;
  }
  if (response != 0) {
    fprintf(stderr,
            "Server rejected connection: protocol version mismatch "
            "(client version: %d)\n",
            GAME_PROTOCOL_VERSION);
    return -1;
  }

  return 0;
}

static void ma_sound_start_wrap(ma_sound *pSound, const char *file, const int line) {
  ma_result result = ma_sound_start(pSound);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "[ma_sound_start] Failed (%s:%d) -> result = %d\n", file, line, result);
  }
}

static SoundContext_t *g_sound_context = NULL;
static atomic_bool g_audio_needs_restart;
static atomic_bool g_audio_shutting_down;

static void on_audio_device_notification(const ma_device_notification *pNotification) {
  if (!g_sound_context || atomic_load(&g_audio_shutting_down))
    return;
  if (pNotification->type == ma_device_notification_type_stopped) {
    verbose_puts("Audio device stopped; will attempt restart");
    atomic_store(&g_audio_needs_restart, true);
  }
}

static void detect_player_changes(const GameState_t *gs, bool was_connected[MAX_PLAYERS],
                                  bool joined[MAX_PLAYERS], bool left[MAX_PLAYERS]) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    bool now = gs->player[i].is_connected;
    joined[i] = !was_connected[i] && now;
    left[i] = was_connected[i] && !now;
    was_connected[i] = now;
  }
}

typedef enum {
  GAME_SEL_ERROR = 0,
  GAME_SEL_SUCCESS = 1,
  GAME_SEL_BACK = 2,
} EGameSelResult_t;

static EGameSelResult_t handle_game_selection(const PlayerConfig_t *player_config,
                                              SocketContext_t *socket_context, const int8_t my_id,
                                              GameState_t *game_state, ClientState_t *client_state,
                                              SdlContext_t *sdl_context, Font_t *font,
                                              const SoundContext_t *sound_context,
                                              LinkWidget_t **links, const Path_t *path) {
  // (void)player_config;

  uint8_t n_clients = 0;

  ButtonWidget_t *game_choice_button[MAX_CHOICES] = {0};

  // TRANSLATORS: Name of a poker variant. Usually left untranslated.
  ButtonWidget_t *button_deuces_wild =
      button_widget_create(_("Deuces Wild"), (EColor_t){COLOR_WHITE, COLOR_BROWN},
                           font->fonts[FONT_BOLD], (SDL_Keycode)0);

  bool dealing = true;

  layout_links(links, LINK_DEFS_COUNT);

  static uint8_t saved_n_clients = 0;
  TCPsocket sock = socket_context->sock;

  UIRegistry_t registry = {0};
  ui_register(&registry, &button_deuces_wild->base);

  for (int i = 0; i < MAX_CHOICES; i++) {
    game_choice_button[i] =
        button_widget_create(game_choices[i].str, (EColor_t){COLOR_BLACK, COLOR_YELLOW},
                             font->fonts[FONT_BOLD], (SDL_Keycode)0);
    ui_register(&registry, &game_choice_button[i]->base);
  }

  UITable_t gc_table = {0};
  ui_table_begin(&gc_table, 0, g_viewport.y + MARGIN, 2);
  for (int i = 0; i < MAX_CHOICES; i++)
    ui_table_add(&gc_table, i / 2, i % 2, &game_choice_button[i]->base);
  int gc_total_w = gc_table.col_width[0] + gc_table.col_width[1] + gc_table.col_spacing;
  gc_table.x = (g_viewport.w - gc_total_w) / 2;
  ui_table_layout(&gc_table);

  int gc_bottom = gc_table.y;
  for (int r = 0; r < gc_table.rows; r++)
    gc_bottom += gc_table.row_height[r] + gc_table.row_spacing;
  gc_bottom -= gc_table.row_spacing;

  button_deuces_wild->base.rect.x = (g_viewport.w - button_deuces_wild->base.rect.w) / 2;
  button_deuces_wild->base.rect.y = gc_bottom + MARGIN;

  NickWidget_t *nick_widgets[MAX_PLAYERS] = {0};
  DealerWidget_t *dealer_widgets[MAX_PLAYERS] = {0};
  int8_t selected_nick = -1;
  PingWidget_t *ping_widgets[MAX_PLAYERS] = {0};
  UITable_t table = {0};
  bool table_needs_rebuild = true;

  TextWidget_t *connected_tw =
      text_widget_create(_("Players"), font->fonts[FONT_BOLD], get_color(COLOR_BLACK));
  ui_register(&registry, &connected_tw->base);

  TextWidget_t *dealer_label_tw =
      text_widget_create(_("Dealer"), font->fonts[FONT_BOLD], get_color(COLOR_BLACK));
  ui_register(&registry, &dealer_label_tw->base);

  TextWidget_t *ping_label_tw =
      text_widget_create(_("Ping"), font->fonts[FONT_BOLD], get_color(COLOR_BLACK));
  ui_register(&registry, &ping_label_tw->base);

  TextWidget_t *waiting_players_tw = text_widget_create(
      _("Waiting for more players..."), font->fonts[FONT_DEFAULT], get_color(COLOR_WHITE));
  ui_register(&registry, &waiting_players_tw->base);
  waiting_players_tw->base.rect.x = g_center.x;
  waiting_players_tw->base.rect.y = g_viewport.h - 200;

  TextWidget_t *waiting_dealer_tw = text_widget_create(
      _("Waiting for dealer to select game..."), font->fonts[FONT_DEFAULT], get_color(COLOR_WHITE));
  ui_register(&registry, &waiting_dealer_tw->base);
  waiting_dealer_tw->base.rect.x = g_center.x;
  waiting_dealer_tw->base.rect.y = g_viewport.h - 200;

  ButtonWidget_t *btn_kick = button_widget_create(_("Kick"), (EColor_t){COLOR_WHITE, COLOR_BROWN},
                                                  font->fonts[FONT_BOLD], (SDL_Keycode)0);
  ButtonWidget_t *btn_ban = button_widget_create(_("Ban"), (EColor_t){COLOR_WHITE, COLOR_BROWN},
                                                 font->fonts[FONT_BOLD], (SDL_Keycode)0);
  btn_kick->base.rect.x = g_viewport.w / 10;
  btn_kick->base.rect.y = g_viewport.h * 82 / 100;
  btn_ban->base.rect.x = btn_kick->base.rect.x + btn_kick->base.rect.w + 16;
  btn_ban->base.rect.y = btn_kick->base.rect.y;
  ui_register(&registry, &btn_kick->base);
  ui_register(&registry, &btn_ban->base);

  static bool was_connected[MAX_PLAYERS] = {0};

  EGameSelResult_t result = GAME_SEL_SUCCESS;

  PathconfLimits_t img_limits = {0};
  get_pathconf_limits(path->data, &img_limits);
  char *back_img_path = join_paths(img_limits.path_max, path->data, "images", "arrow_back.png");
  ImageWidget_t *back_img = image_widget_create(back_img_path, back_btn_size, back_btn_size);
  free(back_img_path);
  if (back_img) {
    back_img->base.rect.x = g_viewport.x + g_viewport.w - back_btn_size - 20;
    back_img->base.rect.y = g_viewport.y + g_viewport.h / 2;
    ui_register(&registry, &back_img->base);
  }

  Uint32 anim_start = SDL_GetTicks();

  while (game_state->at_menu) {
    ERecvStatus_t recv_status = recv_game_state(socket_context, game_state, client_state, my_id);
    if (recv_status == RECV_ERROR) {
      result = GAME_SEL_ERROR;
      goto cleanup;
    }

    clear_screen(sdl_context->renderer);

    int mx, my;
    SDL_GetMouseState(&mx, &my);

    float lx, ly;
    SDL_RenderWindowToLogical(sdl_context->renderer, mx, my, &lx, &ly);

    SDL_Point mouse_pos = {(int)lx, (int)ly};

    if (back_img) {
      float t = (SDL_GetTicks() - anim_start) / 1000.0f;
      if (t > 1.0f)
        t = 1.0f;
      int start_y = g_viewport.y + g_viewport.h * 2 / 3;
      int end_y = g_viewport.y + g_viewport.h - back_btn_size - 20;
      back_img->base.rect.y = start_y + (int)(t * (end_y - start_y));
    }

    for (int i = 0; i < MAX_CHOICES; i++)
      game_choice_button[i]->base.hovered =
          SDL_PointInRect(&mouse_pos, &game_choice_button[i]->base.rect) &&
          game_choice_button[i]->interactive;

    button_deuces_wild->base.hovered =
        SDL_PointInRect(&mouse_pos, &button_deuces_wild->base.rect) &&
        button_deuces_wild->interactive;

    if (back_img)
      back_img->base.hovered = SDL_PointInRect(&mouse_pos, &back_img->base.rect);

    for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
      links[i]->base.hovered = SDL_PointInRect(&mouse_pos, &links[i]->base.rect);

    bool joined[MAX_PLAYERS] = {0};
    bool left[MAX_PLAYERS] = {0};
    detect_player_changes(game_state, was_connected, joined, left);
    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (left[i]) {
        if (nick_widgets[i]) {
          ui_unregister(&registry, &nick_widgets[i]->base);
          ui_widget_destroy(&nick_widgets[i]->base);
          nick_widgets[i] = NULL;
        }
        if (selected_nick == i)
          selected_nick = -1;

        if (dealer_widgets[i]) {
          ui_unregister(&registry, &dealer_widgets[i]->base);
          ui_widget_destroy(&dealer_widgets[i]->base);
          dealer_widgets[i] = NULL;
        }

        if (ping_widgets[i]) {
          ui_unregister(&registry, &ping_widgets[i]->base);
          ui_widget_destroy(&ping_widgets[i]->base);
          ping_widgets[i] = NULL;
        }
      }
      if (joined[i] || left[i])
        table_needs_rebuild = true;
    }

    static int8_t prev_dealer_id = -1;

    if (game_state->dealer_id != prev_dealer_id) {
      for (int i = 0; i < MAX_PLAYERS; i++) {
        if (dealer_widgets[i])
          dealer_widget_set(dealer_widgets[i], game_state->dealer_id == i);
      }
      if (game_state->dealer_id == my_id && player_config->turn_notify)
        ma_sound_start_checked(&sound_context->sounds[SND_MY_TURN].sound);
      prev_dealer_id = game_state->dealer_id;
    }

    // printf("game_state is connected %d\n", game_state->player[i].is_connected);
    if (table_needs_rebuild) {
      ui_table_begin(&table, g_viewport.w / 10, g_viewport.h / 2, 3);
      ui_table_add(&table, 0, 0, &connected_tw->base);
      ui_table_add(&table, 0, 1, &dealer_label_tw->base);
      ui_table_add(&table, 0, 2, &ping_label_tw->base);

      Player_t *client = &game_state->player[my_id];
      Player_t *start = client;
      n_clients = 0;
      int row = 1;

      do {
        int id = client->id;

        /* nick */
        if (!nick_widgets[id]) {
          nick_widgets[id] = nick_widget_create(client->nick, game_state->player[id].id,
                                                font->fonts[FONT_BOLD], get_color(COLOR_WHITE));
          nick_widgets[id]->highlight = (id == my_id);
          nick_widgets[id]->selectable = (game_state->player[my_id].is_admin && id != my_id);
          ui_register(&registry, &nick_widgets[id]->base);
        }

        /* dealer indicator */
        if (!dealer_widgets[id]) {
          dealer_widgets[id] = dealer_widget_create(game_state->dealer_id == id);
          ui_register(&registry, &dealer_widgets[id]->base);
        }

        /* ping */
        if (!ping_widgets[id]) {
          ping_widgets[id] =
              ping_widget_create(client_state->ping_times[id], font->fonts[FONT_BOLD]);
          ui_register(&registry, &ping_widgets[id]->base);
        }

        /* table */
        ui_table_add(&table, row, 0, &nick_widgets[id]->base);
        ui_table_add(&table, row, 1, &dealer_widgets[id]->base);
        ui_table_add(&table, row, 2, &ping_widgets[id]->base);

        row++;
        n_clients++;

        client = get_next_connected_client(game_state->player, client->id);
      } while (client && client != start);
      table.dirty = true; // force layout after rebuild
      table_needs_rebuild = false;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (!ping_widgets[i])
        continue;
      int old_w = ping_widgets[i]->base.rect.w;
      ping_widget_update(ping_widgets[i], client_state->ping_times[i]);
      if (ping_widgets[i]->base.rect.w != old_w)
        table.dirty = true;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (!nick_widgets[i])
        continue;
      nick_widgets[i]->base.hovered = SDL_PointInRect(&mouse_pos, &nick_widgets[i]->base.rect);
    }

    bool admin = game_state->player[my_id].is_admin;
    btn_kick->base.enabled = admin && selected_nick >= 0;
    btn_kick->interactive = admin && selected_nick >= 0;
    btn_kick->base.hovered =
        btn_kick->interactive && SDL_PointInRect(&mouse_pos, &btn_kick->base.rect);
    btn_ban->base.enabled = admin && selected_nick >= 0;
    btn_ban->interactive = admin && selected_nick >= 0;
    btn_ban->base.hovered =
        btn_ban->interactive && SDL_PointInRect(&mouse_pos, &btn_ban->base.rect);

    bool dealing_enabled = game_state->dealer_id == my_id && n_clients > 1;
    bool dw = button_deuces_wild->base.selected;
    for (int i = 0; i < MAX_CHOICES; i++) {
      game_choice_button[i]->base.enabled = dealing;
      bool incompatible = dw && !game_choices[i].deuces_wild_compatible;
      game_choice_button[i]->interactive = dealing_enabled && !incompatible;
    }

    button_deuces_wild->interactive = dealing_enabled;
    button_deuces_wild->base.enabled = dealing;

    if (table.dirty) {
      ui_table_layout(&table);
      // shift ping column (col 2) to center x
      int ping_x = g_center.x;
      for (int r = 0; r < table.rows; r++) {
        UIWidget_t *w = table.cells[r][2];
        if (w)
          w->rect.x = ping_x;
      }
      table.dirty = false;
      table_needs_rebuild = false;
    }

    waiting_players_tw->base.enabled = dealing && (n_clients == 1);
    waiting_dealer_tw->base.enabled = dealing && (n_clients > 1 && game_state->dealer_id != my_id);

    ui_render_all(&registry);
    ui_table_draw_row_separators(&table, sdl_context->renderer);

    // int wx, wy, rx, ry;
    // SDL_GetWindowSize(sdl_context->window, &wx, &wy);
    // SDL_GetRendererOutputSize(sdl_context->renderer, &rx, &ry);
    // SDL_Log("window: %dx%d  renderer output: %dx%d  logical: %dx%d",
    // wx, wy, rx, ry, LOGICAL_WIDTH, LOGICAL_HEIGHT);

    // int lw, lh;
    // SDL_RenderGetLogicalSize(sdl_context->renderer, &lw, &lh);
    // SDL_Log("logical size from renderer: %dx%d", lw, lh);

    /* Refresh mouse_pos immediately before event polling so click checks
     * use the most current position, not the one from the top of the frame. */
    {
      int rmx, rmy;
      SDL_GetMouseState(&rmx, &rmy);
      float rlx, rly;
      SDL_RenderWindowToLogical(sdl_context->renderer, rmx, rmy, &rlx, &rly);
      mouse_pos.x = (int)rlx;
      mouse_pos.y = (int)rly;
    }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {

      case SDL_QUIT:
        result = false;
        goto cleanup;

      case SDL_MOUSEBUTTONDOWN: {
        if (e.button.button == SDL_BUTTON_LEFT) {
          if (back_img && SDL_PointInRect(&mouse_pos, &back_img->base.rect)) {
            result = GAME_SEL_BACK;
            goto cleanup;
          }
          for (int i = 0; i < MAX_CHOICES; i++) {
            if (SDL_PointInRect(&mouse_pos, &game_choice_button[i]->base.rect) &&
                game_choice_button[i]->interactive && game_state->dealer_id == my_id) {
              if (send_game_select(sock, game_choices[i].game_type,
                                   button_deuces_wild->base.selected) == 0) {
                dealing = false;
                break;
              } else {
                result = GAME_SEL_ERROR;
                goto cleanup;
              }
            }
          }
          for (size_t i = 0; i < LINK_DEFS_COUNT; i++) {
            if (SDL_PointInRect(&mouse_pos, &links[i]->base.rect))
              if (SDL_OpenURL(links[i]->url) == -1)
                fputs(SDL_GetError(), stderr);
          }
          if (SDL_PointInRect(&mouse_pos, &button_deuces_wild->base.rect) &&
              button_deuces_wild->interactive) {
            button_deuces_wild->click.start_time = SDL_GetTicks();
            button_deuces_wild->base.selected = !button_deuces_wild->base.selected;
          }
          if (game_state->player[my_id].is_admin) {
            for (int8_t i = 0; i < MAX_PLAYERS; i++) {
              if (!nick_widgets[i] || !nick_widgets[i]->selectable)
                continue;
              if (SDL_PointInRect(&mouse_pos, &nick_widgets[i]->base.rect)) {
                if (selected_nick >= 0 && nick_widgets[selected_nick])
                  nick_widgets[selected_nick]->base.selected = false;
                if (selected_nick == i)
                  selected_nick = -1;
                else
                  selected_nick = i;
                if (selected_nick >= 0)
                  nick_widgets[selected_nick]->base.selected = true;
                break;
              }
            }
            if (selected_nick >= 0) {
              if (btn_kick->interactive && SDL_PointInRect(&mouse_pos, &btn_kick->base.rect)) {
                send_kick_player(sock, selected_nick);
                selected_nick = -1;
              } else if (btn_ban->interactive && SDL_PointInRect(&mouse_pos, &btn_ban->base.rect)) {
                send_ban_player(sock, selected_nick);
                selected_nick = -1;
              }
            }
          }
        }
      } break;
      case SDL_KEYDOWN:
        switch (e.key.keysym.sym) {

        case SDLK_RETURN:
          if (e.key.keysym.mod & KMOD_ALT)
            toggle_fullscreen(sdl_context);
          break;

        case SDLK_F11:
          toggle_fullscreen(sdl_context);
          break;

        default:
          break;
        }
        break;
      default:
        break;
      }
    }

    if (dealing == true) {
      if (saved_n_clients < n_clients && saved_n_clients != 0)
        ma_sound_start_checked(&sound_context->sounds[SND_SERVER_JOIN].sound);
      saved_n_clients = n_clients;

      for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
        ui_widget_render(&links[i]->base);

    } else {
      // Show dealing screen immediately after click
      show_loading_screen(sdl_context->renderer, font->fonts[FONT_TITLE], _("Dealing..."));
    }
    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

cleanup:
  ui_destroy_all(&registry);
  return result;
}

int send_player_action(ClientState_t *client_state, TCPsocket sock, uint8_t action,
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

int send_discards_request_new_cards(TCPsocket sock, const uint8_t *discard_indices, uint8_t count) {
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

static void make_human_readable_card(DH_Card *card, CardWidget_t *cw) {
  const char *face = DH_get_card_face_str(card->face_val);
  const char *suit = DH_get_card_unicode_suit(*card);
  cw->textColor = (card->suit == DH_SUIT_HEARTS || card->suit == DH_SUIT_DIAMONDS)
                      ? get_color(COLOR_RED)
                      : get_color(COLOR_BLACK);
  snprintf(cw->text, sizeof(cw->text), "%s%s", face, suit);
  if (strlen(cw->text) == 0) {
    fprintf(stderr, "%s:String length 0\n", __func__);
    exit(EXIT_FAILURE);
  }
}

static void create_card_context(CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE],
                                const int start_i, Player_t *players_array,
                                const SDL_Point *player_pos, TTF_Font *font, const int my_id,
                                const bool deuces_wild) {
  memset(card_context, 0, sizeof(CardWidget_t) * MAX_PLAYERS * MAX_HAND_SIZE);
  Player_t *turn = &players_array[start_i];
  Player_t *starting_turn = turn;
  do {
    for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
      // printf("%d\n", __LINE__);
      const int id = turn->id;
      DH_Card *card = &(turn->hand.card)[card_n];
      const SDL_Point card_pos = {
          player_pos[id].x + card_n * (CARD_W + CARD_PADDING),
          player_pos[id].y,
      };

      CardWidget_t *cw = &card_context[id][card_n];
      card_widget_init(cw, font);
      cw->base.rect.x = card_pos.x;
      cw->base.rect.y = card_pos.y;
      cw->my_card = (id == my_id);

      cw->is_null = DH_is_card_null(*card);
      if (!turn->in && !cw->is_null)
        memcpy(card, &DH_card_back, sizeof(DH_card_back));

      cw->is_back = DH_is_card_back(*card);

      if (!cw->is_back && !cw->is_null) {
        // Use a condition here so is_wild is not set to false if the card has
        // been changed
        if (!cw->is_wild && deuces_wild)
          cw->is_wild = card->face_val == DH_CARD_TWO;

        make_human_readable_card(card, cw);
      }
    }
    turn = get_next_connected_client(players_array, turn->id);
  } while (turn && turn != starting_turn);
}

/* After winner_declared: mark the 5 cards that form the winning hand.
 * For non-community games the server already trimmed to the best 5 (positions
 * 0-4, rest null), so every non-null card is a winning card.
 * For community card games (Hold'em, Omaha) we recompute the best hand on the
 * client and match cards back to their hand positions by face value and suit.
 * Community cards are marked on every player's context; layout_board_cards has
 * already set is_null=true on non-board player slots, so they won't render. */
static void mark_winning_cards(CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE],
                               Player_t *players_array, const GameChoice_t *game_choice) {
  if (!game_choice)
    return;

  bool is_community = false;
  for (int i = 0; i < MAX_HAND_SIZE; i++) {
    if (game_choice->card_slot[i] == CARD_SLOT_COMMUNITY) {
      is_community = true;
      break;
    }
  }

  for (int p = 0; p < MAX_PLAYERS; p++) {
    if (!players_array[p].is_connected || !players_array[p].winner)
      continue;

    if (!is_community) {
      // Server sorted the best 5 into positions 0-4; null beyond that
      for (int c = 0; c < MAX_HAND_SIZE; c++) {
        if (!card_context[p][c].is_null && !card_context[p][c].is_back)
          card_context[p][c].is_winning = true;
      }
      continue;
    }

    // Recompute the best 5-card hand for this winner
    POKEVAL_Hand_5 best;
    if (game_choice->game_type == game_choices[OMAHA].game_type)
      best = POKEVAL_hand5_omaha(&players_array[p].hand);
    else
      best = POKEVAL_hand5_from_hand7(&players_array[p].hand);

    for (int c = 0; c < MAX_HAND_SIZE; c++) {
      DH_Card card = players_array[p].hand.card[c];
      if (DH_is_card_null(card) || DH_is_card_back(card))
        continue;
      for (int b = 0; b < POKEVAL_HAND_SIZE; b++) {
        if (card.face_val == best.card[b].face_val && card.suit == best.card[b].suit) {
          if (game_choice->card_slot[c] == CARD_SLOT_HOLE) {
            card_context[p][c].is_winning = true;
          } else {
            // Community card: mark on all players; non-board slots are is_null so won't render
            for (int q = 0; q < MAX_PLAYERS; q++)
              card_context[q][c].is_winning = true;
          }
          break;
        }
      }
    }
  }
}

/* Reposition community cards to the board area below player 0
 * and suppress those positions from all other player hand slots.
 * community_start is the first card index that is a community card. */
static void layout_board_cards(CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE],
                               const int board_player_id, const SDL_Point *player_pos,
                               const int community_start) {
  const int board_x = player_pos[0].x + (int)(CARD_W * 0.5f);
  /* Position one card-height above the status panel (which starts at g_center.y) */
  const int board_y = g_center.y - CARD_H * 2;

  for (int card_n = community_start; card_n < MAX_HAND_SIZE; card_n++) {
    int slot = card_n - community_start;
    SDL_Rect rect = {board_x + slot * (CARD_W + CARD_PADDING), board_y, CARD_W, CARD_H};
    card_context[board_player_id][card_n].base.rect = rect;

    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (i == board_player_id)
        continue;
      card_context[i][card_n].is_null = true;
    }
  }
}

void layout_cards(CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE], Player_t *players_array,
                  const SDL_Point *player_pos) {

  Player_t *starting_turn = NULL;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (players_array[i].is_connected) {
      starting_turn = &players_array[i];
      break;
    }
  }
  if (!starting_turn)
    return;
  Player_t *turn = starting_turn;
  do {
    for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
      const int id = turn->id;
      SDL_Rect rect = {player_pos[id].x + card_n * (CARD_W + CARD_PADDING), player_pos[id].y,
                       CARD_W, CARD_H};
      card_context[id][card_n].base.rect = rect;
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
    return;
  }

  if (anim->duration <= 0) {
    anim->active = false;
    return;
  }

  uint32_t now = SDL_GetTicks();
  float progress = (now - anim->start_time) / (float)anim->duration;

  if (progress >= 1.0f) {
    progress = 1.0f;
    anim->active = false;
  }

  float fx = anim->start.x + (anim->end.x - anim->start.x) * progress;
  float fy = anim->start.y + (anim->end.y - anim->start.y) * progress;

  SDL_Rect dst = {.x = (int)fx, .y = (int)fy, .w = coin_px, .h = coin_px};

  SDL_RenderCopy(renderer, anim->texture, NULL, &dst);
}

enum {
  CHECK,
  BET,
  FOLD,
  CALL,
  RAISE,
  COMPLETE,
  DISCARD,
  MAX_ACTIONS,
};

static void layout_amount_buttons(ButtonWidget_t **b, const size_t count) {
  int left_margin = 500;
  for (size_t i = 0; i < count; i++) {
    b[i]->base.rect.x = left_margin;
    b[i]->base.rect.y = g_viewport.h - (b[i]->base.rect.h * 3);
    left_margin += b[i]->base.rect.w + 10;
  }
}

typedef struct {
  const char *text;
  SDL_Keycode key;
  bool secondary;
} ActionButtonAttrs;

ActionButtonAttrs action_button_attrs[MAX_ACTIONS] = {
    [CHECK] = {N_("Check"), SDLK_c, false},    [BET] = {N_("Bet"), SDLK_b, false},
    [FOLD] = {N_("Fold"), SDLK_f, false},      [CALL] = {N_("Call"), SDLK_c, false},
    [RAISE] = {N_("Raise"), SDLK_r, false},    [COMPLETE] = {N_("Complete"), SDLK_r, false},
    [DISCARD] = {N_("Discard"), SDLK_d, true},
};

static void layout_action_buttons(ButtonWidget_t **b, ActionButtonAttrs *attr) {
  for (int i = 0; i < MAX_ACTIONS; i++) {
    b[i]->base.rect.y = g_viewport.h - (b[i]->base.rect.h * 5);
    if (attr[i].secondary)
      b[i]->base.rect.y += b[i]->base.rect.h + 10;
  }
}

static void layout_player_pos(SDL_Point *player_pos) {
  SDL_Rect vp = g_viewport;

  int right_x = vp.x + vp.w - (CARD_W * 7 + CARD_PADDING * 7 + MARGIN);

  player_pos[0].x = vp.x + MARGIN;
  player_pos[0].y = vp.y + CARD_H * 4;

  player_pos[1].x = vp.x + 20;
  player_pos[1].y = vp.y + CARD_H;

  player_pos[2].x = right_x;
  player_pos[2].y = vp.y + CARD_H;

  player_pos[3].x = right_x;
  player_pos[3].y = vp.y + CARD_H * 4;

  player_pos[4].x = right_x;
  player_pos[4].y = vp.y + CARD_H * 7;
}

typedef struct {
  SDL_Point offset;
  SDL_Rect rect;
} CoinInPot_t;

static void layout_coins(CoinInPot_t *coins, SDL_Point *p, int count) {
  for (int i = 0; i < count; i++) {
    coins[i].rect.w = coin_px;
    coins[i].rect.h = coin_px;

    coins[i].rect.x = p->x + coins[i].offset.x - coin_px / 2;
    coins[i].rect.y = p->y + coins[i].offset.y - coin_px / 2;
  }
}

static void layout_table_center(SDL_Point *p) {
  p->x = g_center.x;
  p->y = g_center.y;
}

static inline int right_align(int width) { return g_viewport.x + g_viewport.w - width - MARGIN; }

static void layout_indicator(Indicator_t *ind, int x, int y) {
  ui_widget_place(&ind->base, x, y); // sets base.rect.x/y

  ind->rx = ind->base.rect.w / 2;
  ind->ry = ind->base.rect.h / 2;

  ind->cx = ind->base.rect.x + ind->rx;
  ind->cy = ind->base.rect.y + ind->ry;
}

static void layout_game_name_indicator(Indicator_t *ind) {
  int x = right_align(ind->base.rect.w);
  int y = g_viewport.y + g_viewport.h - 300;

  layout_indicator(ind, x, y);
}

static void layout_deuces_wild_indicator(Indicator_t *ind) {
  int x = right_align(ind->base.rect.w);
  int y = g_viewport.y + g_viewport.h - 200;

  layout_indicator(ind, x, y);
}

/* Circle timer radius — needed by layout_timer below */
#define CIRCLE_TIMER_R 50

static void layout_timer(SDL_Point *p) {
  p->x = g_center.x;
  /* Bottom of circle aligns with MARGIN from viewport bottom */
  p->y = g_viewport.h - MARGIN - CIRCLE_TIMER_R;
}

static void draw_filled_circle(SDL_Renderer *r, int cx, int cy, int radius) {
  if (radius <= 0)
    return;

  for (int y = -radius; y <= radius; y++) {
    float dy = (float)y / (float)radius;
    float inside = 1.0f - dy * dy;
    if (inside < 0.0f)
      continue;

    int x = (int)(radius * sqrtf(inside) + 0.5f);
    SDL_RenderDrawLine(r, cx - x, cy + y, cx + x, cy + y);
  }
}

/* Circle timer ------------------------------------------------------------- */

/*
 * render_circle_timer - draw a 100px-diameter pie-style countdown circle.
 *
 * fill_ratio : 1.0 = full (just started, no fill), 0.0 = empty (all filled)
 *
 * The brown wedge grows clockwise from 12 o'clock as time elapses.
 * A 10px 3D border ring (light on top-left, dark on bottom-right) frames it.
 */
static void render_circle_timer(SDL_Renderer *renderer, SDL_Point center, float fill_ratio) {
  const int cx = center.x;
  const int cy = center.y;
  const int outer_r = CIRCLE_TIMER_R; /* 50 */
  const int border = 10;
  const int inner_r = outer_r - border; /* 40 */

  /* --- 3D border ring ---------------------------------------------------- */
  for (int y = cy - outer_r; y <= cy + outer_r; y++) {
    int dy = y - cy;
    float dy_f = (float)dy;
    int outer_hw = (int)sqrtf((float)(outer_r * outer_r) - dy_f * dy_f);
    float inner_r2 = (float)(inner_r * inner_r);
    int inner_hw = (dy_f * dy_f <= inner_r2) ? (int)sqrtf(inner_r2 - dy_f * dy_f) : -1;

    for (int x = cx - outer_hw; x <= cx + outer_hw; x++) {
      int dx = x - cx;
      /* skip pixels inside the inner circle */
      if (inner_hw >= 0 && dx >= -inner_hw && dx <= inner_hw)
        continue;
      /* 3D shading: dot with (-1,-1) direction; bright top-left, dark bottom-right */
      float t = (-(float)dx - dy_f) / ((float)outer_r * 1.414f);
      if (t > 1.0f)
        t = 1.0f;
      if (t < -1.0f)
        t = -1.0f;
      t = (t + 1.0f) * 0.5f;                     /* 0..1 */
      uint8_t c = (uint8_t)(70.0f + t * 185.0f); /* 70 (dark gray) → 255 (white) */
      SDL_SetRenderDrawColor(renderer, c, c, c, 255);
      SDL_RenderDrawPoint(renderer, x, y);
    }
  }

  /* --- inner circle background (match table green) ----------------------- */
  SDL_SetRenderDrawColor(renderer, 0, 125, 0, 255);
  for (int y = cy - inner_r; y <= cy + inner_r; y++) {
    int dy = y - cy;
    int hw = (int)sqrtf((float)(inner_r * inner_r - dy * dy));
    SDL_RenderDrawLine(renderer, cx - hw, y, cx + hw, y);
  }

  /* --- brown pie wedge (elapsed time, clockwise from 12 o'clock) --------- */
  float sweep = (1.0f - fill_ratio) * 2.0f * (float)M_PI;
  if (sweep > 0.001f) {
    SDL_SetRenderDrawColor(renderer, 101, 67, 33, 255); /* brown */
    for (int y = cy - inner_r; y <= cy + inner_r; y++) {
      int dy = y - cy;
      int hw = (int)sqrtf((float)(inner_r * inner_r - dy * dy));
      for (int x = cx - hw; x <= cx + hw; x++) {
        int dx = x - cx;
        /* angle from 12 o'clock, increasing clockwise */
        float a = atan2f((float)dx, -(float)dy);
        if (a < 0.0f)
          a += 2.0f * (float)M_PI;
        if (a <= sweep)
          SDL_RenderDrawPoint(renderer, x, y);
      }
    }
  }
}
/* end circle timer --------------------------------------------------------- */

/*
 * word_wrap_status - break `text` into lines that fit within `max_w` pixels
 * using `font`.  Writes at most `max_lines` NUL-terminated strings into `out`
 * (each slot is LEN_STATUS_STR bytes).  Returns the number of lines produced.
 *
 * Words are split on spaces.  A single word that is wider than `max_w` is
 * placed on its own line anyway (no mid-word break at this resolution).
 */
static int word_wrap_status(TTF_Font *font, const char *text, int max_w, char out[][LEN_STATUS_STR],
                            int max_lines) {
  int n = 0;
  const char *p = text;

  while (*p && n < max_lines) {
    const char *line_start = p;
    const char *last_break = p; /* end of last word-run that still fits */

    while (*p) {
      /* skip whitespace then scan one word */
      const char *ws = p;
      while (*ws == ' ')
        ws++;
      const char *we = ws;
      while (*we && *we != ' ')
        we++;

      /* measure from line_start to end of this word */
      int span = (int)(we - line_start);
      if (span >= LEN_STATUS_STR)
        span = LEN_STATUS_STR - 1;

      char tmp[LEN_STATUS_STR];
      memcpy(tmp, line_start, (size_t)span);
      tmp[span] = '\0';

      int w = 0;
      TTF_SizeUTF8(font, tmp, &w, NULL);

      if (w <= max_w || last_break == line_start) {
        /* fits (or it's the very first word — always accept it) */
        last_break = we;
        p = we;
        if (!*p)
          break;
      } else {
        /* this word pushes over; wrap before it */
        break;
      }
    }

    /* copy [line_start, last_break) trimming trailing spaces */
    int len = (int)(last_break - line_start);
    while (len > 0 && line_start[len - 1] == ' ')
      len--;
    if (len >= LEN_STATUS_STR)
      len = LEN_STATUS_STR - 1;
    memcpy(out[n], line_start, (size_t)len);
    out[n][len] = '\0';
    n++;

    /* advance past whitespace before the next line */
    p = last_break;
    while (*p == ' ')
      p++;
  }

  if (n == 0) {
    out[0][0] = '\0';
    n = 1;
  }

  return n;
}

static void render_text_pot(const char *text, const SDL_Point center, const Font_t *font) {
  if (!text)
    text = "";

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font->fonts[FONT_DEFAULT_BOLD], *text ? text : " ",
                                                get_color(COLOR_BLACK));
  if (!surface) {
    fprintf(stderr, "TTF_RenderUTF8_Blended error: %s\n", TTF_GetError());
    return;
  }

  SDL_Renderer *renderer = g_sdl_context->renderer;
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    SDL_FreeSurface(surface);
    fprintf(stderr, "SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
    return;
  }

  int text_w = surface->w;
  int text_h = surface->h;

  /* Background circle */
  int PAD = 14;
  int radius = (text_w > text_h ? text_w : text_h) / 2 + PAD;
  if (radius < 24)
    radius = 24;

  SDL_SetRenderDrawBlendMode(g_sdl_context->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
  draw_filled_circle(renderer, center.x, center.y, radius);

  /* Text rect centered */
  SDL_Rect text_rect = {center.x - text_w / 2, center.y - text_h / 2, text_w, text_h};

  SDL_RenderCopy(renderer, texture, NULL, &text_rect);

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

static bool handle_game_logic(const PlayerConfig_t *player_config, SocketContext_t *socket_context,
                              const GameSettings_t *game_settings, GameState_t *game_state,
                              SdlContext_t *sdl_context, const Font_t *font, Path_t *path,
                              const SoundContext_t *sound_context) {
  card_widget_select_back_for_game();

  ClientState_t client_state = {0};

  NickWidget_t *game_nick_widgets[MAX_PLAYERS] = {0};
  ImageWidget_t *game_coin_widgets[MAX_PLAYERS] = {0};
  TextWidget_t *game_coins_tw[MAX_PLAYERS] = {0};
  int8_t selected_nick = -1;

  bool was_connected[MAX_PLAYERS];
  int32_t prev_coins[MAX_PLAYERS];
  for (int i = 0; i < MAX_PLAYERS; i++) {
    was_connected[i] = game_state->player[i].is_connected;
    prev_coins[i] = game_state->player[i].coins;
  }

  SDL_Point player_pos[MAX_PLAYERS] = {0};
  layout_player_pos(player_pos);

#define SIZEOF_STATUS_MSGS 16
  char status_msgs[SIZEOF_STATUS_MSGS][LEN_STATUS_STR] = {0};
  char last_status_str[LEN_STATUS_STR] = {0};
  TextWidget_t *status_tw[SIZEOF_STATUS_MSGS] = {0};

  ButtonWidget_t *action_bw[MAX_ACTIONS];
  for (int i = 0; i < MAX_ACTIONS; i++) {
    action_bw[i] =
        button_widget_create(action_button_attrs[i].text, (EColor_t){COLOR_BLACK, COLOR_YELLOW},
                             font->fonts[FONT_BOLD], action_button_attrs[i].key);
  }
  layout_action_buttons(action_bw, action_button_attrs);

  TextWidget_t *discard_hint_tw = text_widget_create(
      "You may only discard a maximum of 3 cards", font->fonts[FONT_BOLD], get_color(COLOR_WHITE));
  if (discard_hint_tw)
    ui_widget_place(&discard_hint_tw->base, x_begin_action_button,
                    action_bw[DISCARD]->base.rect.y + CARD_H);
  uint8_t last_max_allowed = 3;

  static const SDL_Keycode bet_hotkeys[MAX_BET_AMOUNTS] = {
      SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8,
  };
  const size_t n_bet_amounts = game_settings->bet_amount_count;
  struct Amount_t {
    uint32_t value;
    SDL_Keycode hotkey;
  } amount[MAX_BET_AMOUNTS];
  for (size_t i = 0; i < n_bet_amounts; i++) {
    amount[i].value = game_settings->bet_amounts[i];
    amount[i].hotkey = bet_hotkeys[i];
  }

  ButtonWidget_t *amount_bw[MAX_BET_AMOUNTS] = {0};

  SDL_Point timer = {0};
  layout_timer(&timer);

  char amount_str[MAX_BET_AMOUNTS][16]; // enough for uint32_t

  for (size_t i = 0; i < n_bet_amounts; i++) {
    snprintf(amount_str[i], sizeof(amount_str[i]), "%" PRIu32, amount[i].value);
    amount_bw[i] = button_widget_create(amount_str[i], (EColor_t){COLOR_WHITE, COLOR_BROWN},
                                        font->fonts[FONT_BOLD], amount[i].hotkey);
  }
  amount_bw[0]->base.selected = true;
  client_state.selected_amount = amount[0].value;
  layout_amount_buttons(amount_bw, n_bet_amounts);

  CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE];

  UIRegistry_t registry = {0};

  ButtonWidget_t *game_btn_kick = button_widget_create(
      _("Kick"), (EColor_t){COLOR_WHITE, COLOR_BROWN}, font->fonts[FONT_BOLD], (SDL_Keycode)0);
  ButtonWidget_t *game_btn_ban = button_widget_create(
      _("Ban"), (EColor_t){COLOR_WHITE, COLOR_BROWN}, font->fonts[FONT_BOLD], (SDL_Keycode)0);
  game_btn_kick->base.rect.x = g_viewport.w / 10;
  /* Position above the status message panel, which starts at g_center.y */
  game_btn_kick->base.rect.y = g_center.y - game_btn_kick->base.rect.h - 20;
  game_btn_ban->base.rect.x = game_btn_kick->base.rect.x + game_btn_kick->base.rect.w + 16;
  game_btn_ban->base.rect.y = game_btn_kick->base.rect.y;
  ui_register(&registry, &game_btn_kick->base);
  ui_register(&registry, &game_btn_ban->base);

  Indicator_t *indicator_deuces_wild =
      create_indicator(("Deuces Wild"), font->fonts[FONT_BOLD], COLOR_WHITE, COLOR_BROWN);
  ui_register(&registry, &indicator_deuces_wild->base);

  layout_deuces_wild_indicator(indicator_deuces_wild);

  Indicator_t *indicator_game_name = NULL;

  int running = 1;
  bool cards_created = false;

  Player_t *players_array = game_state->player;
  Player_t *turn = NULL;
  Player_t *starting_turn = NULL;

  Coin_t coin[] = {
      {
          "96x96_front_1907_Saint_Gaudens_gold_coin.png",
      },
      {
          "96x96_front_Gaius-Julius-Caesar-denarius-44-BC-RRC-480-3.png",
      },
      {
          "96x96-1984_rv_marie_curie.png",
      },
      {
          "96x96-head_of_Aphrodite_with_turreted_crown.png",
      },
      {
          "96x96-Marcus Antonius - Cleopatra 32 BC 90020163_front.png",
      },
      {
          "96x96-Marcus Antonius - Cleopatra 32 BC 90020163_back.png",
      },
  };

  const int which_coin = pcg32_boundedrand_r(&rng, ARRAY_SIZE(coin));
  SDL_Texture *coin_tex_front =
      load_coin_texture(sdl_context->renderer, path->data, coin[which_coin].front);

  CoinInPot_t coin_in_pot[MAX_POT_COINS] = {0};

  uint8_t coins = 0;
  CoinAnimation_t coin_anim = {0};
  SDL_Point table_center = {0};
  layout_table_center(&table_center);

  int line_h = TTF_FontHeight(font->fonts[FONT_STATUS_MSG]);

  const int pad_x = 8;
  const int pad_y = 6;

  SDL_Rect msg_panel = {g_viewport.x + 30, g_center.y, 420,
                        (line_h + 2) * SIZEOF_STATUS_MSGS + pad_y * 2};

  for (int i = 0; i < SIZEOF_STATUS_MSGS; i++) {
    status_tw[i] = text_widget_create(" ", font->fonts[FONT_STATUS_MSG], get_color(COLOR_BLACK));
    if (status_tw[i])
      ui_widget_place(&status_tw[i]->base, msg_panel.x + pad_x,
                      msg_panel.y + pad_y + i * (line_h + 2));
  }

  client_state.timer_start = SDL_GetTicks();
  client_state.hourglass_rotate_start = client_state.timer_start;

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

    bool joined[MAX_PLAYERS] = {0};
    bool left[MAX_PLAYERS] = {0};
    detect_player_changes(game_state, was_connected, joined, left);

    for (int8_t id = 0; id < MAX_PLAYERS; id++) {
      if (left[id]) {
        if (selected_nick == id)
          selected_nick = -1;
        if (game_nick_widgets[id]) {
          ui_unregister(&registry, &game_nick_widgets[id]->base);
          ui_widget_destroy(&game_nick_widgets[id]->base);
          game_nick_widgets[id] = NULL;
        }
        if (game_coin_widgets[id]) {
          ui_unregister(&registry, &game_coin_widgets[id]->base);
          ui_widget_destroy(&game_coin_widgets[id]->base);
          game_coin_widgets[id] = NULL;
        }
        if (game_coins_tw[id]) {
          ui_unregister(&registry, &game_coins_tw[id]->base);
          ui_widget_destroy(&game_coins_tw[id]->base);
          game_coins_tw[id] = NULL;
        }
      }
      if (!game_state->player[id].is_connected || game_nick_widgets[id])
        continue;
      char coins_str[24] = {0};
      snprintf(coins_str, sizeof coins_str, "%" PRId32, game_state->player[id].coins);
      game_nick_widgets[id] = nick_widget_create(game_state->player[id].nick, id,
                                                 font->fonts[FONT_BOLD], get_color(COLOR_WHITE));
      game_nick_widgets[id]->highlight = (id == my_id);
      game_nick_widgets[id]->selectable = (game_state->player[my_id].is_admin && id != my_id);
      game_coin_widgets[id] = image_widget_from_texture(coin_tex_front, coin_px / 2, coin_px / 2);
      game_coins_tw[id] =
          text_widget_create(coins_str, font->fonts[FONT_BOLD], get_color(COLOR_BLACK));
      ui_register(&registry, &game_nick_widgets[id]->base);
      ui_register(&registry, &game_coin_widgets[id]->base);
      ui_register(&registry, &game_coins_tw[id]->base);
      prev_coins[id] = game_state->player[id].coins;
    }
    // printf("turn id: %d\n", turn->id);

    if (strcmp(client_state.server_status_str, last_status_str) != 0) {
      snprintf(last_status_str, LEN_STATUS_STR, "%s", client_state.server_status_str);

      int max_w = msg_panel.w - pad_x * 2;
      char wrapped[SIZEOF_STATUS_MSGS][LEN_STATUS_STR];
      int n = word_wrap_status(font->fonts[FONT_STATUS_MSG], client_state.server_status_str, max_w,
                               wrapped, SIZEOF_STATUS_MSGS);

      /* Shift existing lines up by n to make room at the bottom */
      memmove(status_msgs, status_msgs + n, sizeof(status_msgs[0]) * (SIZEOF_STATUS_MSGS - n));
      for (int i = 0; i < n; i++)
        memcpy(status_msgs[SIZEOF_STATUS_MSGS - n + i], wrapped[i], LEN_STATUS_STR);

      for (int i = 0; i < SIZEOF_STATUS_MSGS; i++)
        text_widget_set_text(status_tw[i], *status_msgs[i] ? status_msgs[i] : " ");
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
                          font->fonts[FONT_CARD], my_id, client_state.deuces_wild);
      layout_cards(card_context, players_array, player_pos);
      if (client_state.game_choice) {
        int community_start = -1;
        for (int cs = 0; cs < MAX_HAND_SIZE; cs++) {
          if (client_state.game_choice->card_slot[cs] == CARD_SLOT_COMMUNITY) {
            community_start = cs;
            break;
          }
        }
        if (community_start > 0)
          layout_board_cards(card_context, starting_turn->id, player_pos, community_start);
      }
      if (game_state->winner_declared)
        mark_winning_cards(card_context, game_state->player, client_state.game_choice);
      cards_created = true;
    }

    clear_screen(sdl_context->renderer);

    int mx, my;
    SDL_GetMouseState(&mx, &my);
    float lx, ly;
    SDL_RenderWindowToLogical(sdl_context->renderer, mx, my, &lx, &ly);
    SDL_Point mouse_pos = {(int)lx, (int)ly};

    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (game_nick_widgets[i])
        game_nick_widgets[i]->base.hovered =
            game_nick_widgets[i]->selectable &&
            SDL_PointInRect(&mouse_pos, &game_nick_widgets[i]->base.rect);
    }

    bool game_admin = game_state->player[my_id].is_admin;
    game_btn_kick->base.enabled = game_admin && selected_nick >= 0;
    game_btn_kick->interactive = game_admin && selected_nick >= 0;
    game_btn_kick->base.hovered =
        game_btn_kick->interactive && SDL_PointInRect(&mouse_pos, &game_btn_kick->base.rect);
    game_btn_ban->base.enabled = game_admin && selected_nick >= 0;
    game_btn_ban->interactive = game_admin && selected_nick >= 0;
    game_btn_ban->base.hovered =
        game_btn_ban->interactive && SDL_PointInRect(&mouse_pos, &game_btn_ban->base.rect);

    if (game_state->prev_bet_amount == 0)
      for (size_t i = 0; i < n_bet_amounts; i++)
        amount_bw[i]->interactive = true;

    bool new_coin = false;

    if (game_state->pot > coins * game_settings->bet_amounts[0] && coins < MAX_POT_COINS) {
      coin_in_pot[coins].offset.x = pcg32_boundedrand_r(&rng, POT_BOUNDARY) - POT_BOUNDARY / 2;
      coin_in_pot[coins].offset.y = pcg32_boundedrand_r(&rng, POT_BOUNDARY) - POT_BOUNDARY / 2;
      coins++;
      new_coin = true;
    } else if (game_state->pot == 0) {
      coins = 0;
    }

    if (new_coin) {
      // FIRST: compute rects
      layout_coins(coin_in_pot, &table_center, coins);

      int last = coins - 1;

      // NOW rect is valid
      coin_anim = (CoinAnimation_t){
          .texture = coin_tex_front,
          .start = (SDL_Point){player_pos[turn->id].x, player_pos[turn->id].y},
          .end = (SDL_Point){coin_in_pot[last].rect.x, coin_in_pot[last].rect.y},
          .start_time = SDL_GetTicks(),
          .duration = 300,
          .active = true,
      };
      new_coin = false;
    }

    for (int i = 0; i < coins; i++) {
      if (i == coins - 1 && coin_anim.active) {
        continue; // last coin is currently animated
      }
      SDL_RenderCopy(sdl_context->renderer, coin_tex_front, NULL, &coin_in_pot[i].rect);
    }

    if (game_state->pot > 0)
      render_coin_animation(sdl_context->renderer, &coin_anim);
    // else {
    // coins = 0;
    // coin_anim.active = false;
    //}

    if (!indicator_game_name && client_state.game_choice) {
      indicator_game_name = create_indicator(client_state.game_choice->str, font->fonts[FONT_BOLD],
                                             COLOR_WHITE, COLOR_BROWN);
      ui_register(&registry, &indicator_game_name->base);
      layout_game_name_indicator(indicator_game_name);
    }

    indicator_deuces_wild->base.enabled = client_state.deuces_wild;

    SDL_Rect turn_outline = {0};
    for (int8_t id = 0; id < MAX_PLAYERS; id++) {
      if (!game_nick_widgets[id] || !game_coin_widgets[id] || !game_coins_tw[id])
        continue;
      if (game_state->player[id].coins != prev_coins[id]) {
        char coins_str[24] = {0};
        snprintf(coins_str, sizeof coins_str, "%" PRId32, game_state->player[id].coins);
        text_widget_set_text(game_coins_tw[id], coins_str);
        prev_coins[id] = game_state->player[id].coins;
      }
      UITable_t player_table = {0};
      ui_table_begin(&player_table, player_pos[id].x + CARD_W / 2,
                     player_pos[id].y + (int)(CARD_H * 1.2), 3);
      ui_table_add(&player_table, 0, 0, &game_nick_widgets[id]->base);
      ui_table_add(&player_table, 0, 1, &game_coin_widgets[id]->base);
      ui_table_add(&player_table, 0, 2, &game_coins_tw[id]->base);
      ui_table_layout(&player_table);
      if (id == turn->id && !game_state->winner_declared) {
        int total_w = 0;
        for (int c = 0; c < player_table.cols; c++)
          total_w += player_table.col_width[c] + player_table.col_spacing;
        total_w -= player_table.col_spacing;
        const int pad = 4;
        turn_outline = (SDL_Rect){player_table.x - pad, player_table.y - pad, total_w + pad * 2,
                                  player_table.row_height[0] + pad * 2};
      }
    }

    ui_render_all(&registry);

    if (turn_outline.w > 0) {
      SDL_SetRenderDrawColor(sdl_context->renderer, 255, 215, 0, 255);
      SDL_RenderDrawRect(sdl_context->renderer, &turn_outline);
    }

    SDL_SetRenderDrawColor(sdl_context->renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(sdl_context->renderer, &msg_panel);

    SDL_SetRenderDrawColor(sdl_context->renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(sdl_context->renderer, &msg_panel);

    for (int i = 0; i < SIZEOF_STATUS_MSGS; i++)
      ui_widget_render(&status_tw[i]->base);

    Player_t *player_ptr = starting_turn;
    for (int card_n = 0; card_n < MAX_HAND_SIZE; ++card_n) {
      do {
        // printf("%d\n", __LINE__);
        ui_widget_render(&card_context[player_ptr->id][card_n].base);

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
        client_state.hourglass_rotate_start = client_state.timer_start;

        if (game_state->winner_declared) {
          ma_sound_start_checked(&sound_context->sounds[SND_GAME_OVER].sound);
          client_state.end_game_timer_set = true;
        }

        // Handle timeout: If there was no action by the player, one of these
        // may be set to true
        client_state.bet_check_fold = false;
        client_state.call_raise_fold = false;
        client_state.call_complete_fold = false;
        client_state.complete_check_fold = false;
        client_state.do_discard_draw = false;

        if (my_turn && !game_state->winner_declared) {
          if (player_config->turn_notify)
            ma_sound_start_checked(&sound_context->sounds[SND_MY_TURN].sound);
        }
        client_state.turn_switch = false;
      }
    }

    uint32_t now = SDL_GetTicks();
    int32_t remaining_ms = 0;
    if (game_state->winner_declared) {
      remaining_ms = (int32_t)(client_state.timer_start + game_settings->end_of_game_timeout_ms) -
                     (int32_t)now;
    } else {
      remaining_ms = (int32_t)((client_state.timer_start + game_settings->action_timeout_ms) - now);

      if (client_state.do_discard_draw) {
        for (int i = 0; i < MAX_HAND_SIZE; i++)
          if (turn->hand.card[i].face_val == DH_CARD_ACE ||
              turn->hand.card[i].face_val == DH_CARD_ACE_HIGH) {
            client_state.has_ace = true;
            break;
          }
        uint8_t max_allowed = client_state.has_ace ? 4 : 3;
        action_bw[DISCARD]->interactive = client_state.n_cards_selected <= max_allowed;
        action_bw[DISCARD]->base.rect.x = x_begin_action_button;
        if (!action_bw[DISCARD]->interactive) {
          if (max_allowed != last_max_allowed) {
            char tmp[50];
            snprintf(tmp, sizeof(tmp), "You may only discard a maximum of %d cards", max_allowed);
            text_widget_set_text(discard_hint_tw, tmp);
            last_max_allowed = max_allowed;
          }
          ui_widget_render(&discard_hint_tw->base);
        }
        ui_widget_render(&action_bw[DISCARD]->base);
      } else if (client_state.bet_check_fold || client_state.call_raise_fold ||
                 client_state.call_complete_fold || client_state.complete_check_fold) {
        int x_offset = x_begin_action_button;
        if (client_state.bet_check_fold) {
          action_bw[BET]->base.rect.x = x_offset;
          ui_widget_render(&action_bw[BET]->base);
          x_offset = action_bw[BET]->base.rect.x + action_bw[BET]->base.rect.w + BUTTON_X_SPACING;

          action_bw[CHECK]->base.rect.x = x_offset;
          ui_widget_render(&action_bw[CHECK]->base);
          x_offset =
              action_bw[CHECK]->base.rect.x + action_bw[CHECK]->base.rect.w + BUTTON_X_SPACING;
        } else if (client_state.call_raise_fold) {
          action_bw[CALL]->base.rect.x = x_offset;
          ui_widget_render(&action_bw[CALL]->base);
          x_offset = action_bw[CALL]->base.rect.x + action_bw[CALL]->base.rect.w + BUTTON_X_SPACING;

          action_bw[RAISE]->base.rect.x = x_offset;
          action_bw[RAISE]->interactive = game_state->raises_remaining > 0;
          if (action_bw[RAISE]->interactive)
            ui_widget_render(&action_bw[RAISE]->base);
          x_offset =
              action_bw[RAISE]->base.rect.x + action_bw[RAISE]->base.rect.w + BUTTON_X_SPACING;
        } else if (client_state.call_complete_fold) {
          action_bw[CALL]->base.rect.x = x_offset;
          ui_widget_render(&action_bw[CALL]->base);
          x_offset = action_bw[CALL]->base.rect.x + action_bw[CALL]->base.rect.w + BUTTON_X_SPACING;

          action_bw[COMPLETE]->base.rect.x = x_offset;
          action_bw[COMPLETE]->interactive = game_state->raises_remaining > 0;
          if (action_bw[COMPLETE]->interactive)
            ui_widget_render(&action_bw[COMPLETE]->base);
          x_offset = action_bw[COMPLETE]->base.rect.x + action_bw[COMPLETE]->base.rect.w +
                     BUTTON_X_SPACING;
        } else if (client_state.complete_check_fold) {
          action_bw[COMPLETE]->base.rect.x = x_offset;
          action_bw[COMPLETE]->interactive = game_state->raises_remaining > 0;
          if (action_bw[COMPLETE]->interactive)
            ui_widget_render(&action_bw[COMPLETE]->base);
          x_offset = action_bw[COMPLETE]->base.rect.x + action_bw[COMPLETE]->base.rect.w +
                     BUTTON_X_SPACING;

          action_bw[CHECK]->base.rect.x = x_offset;
          ui_widget_render(&action_bw[CHECK]->base);
          x_offset =
              action_bw[CHECK]->base.rect.x + action_bw[CHECK]->base.rect.w + BUTTON_X_SPACING;
        }
        action_bw[FOLD]->base.rect.x = x_offset;
        ui_widget_render(&action_bw[FOLD]->base);

        // The amount buttons won't be shown if this is true (max raises were reached
        // if the RAISE button is not active).
        if (client_state.bet_check_fold || client_state.complete_check_fold ||
            (client_state.call_raise_fold && action_bw[RAISE]->interactive) ||
            (client_state.call_complete_fold && action_bw[COMPLETE]->interactive)) {
          for (size_t i = 0; i < n_bet_amounts; i++) {
            if (game_state->prev_bet_amount > amount[i].value && action_bw[RAISE]->interactive)
              amount_bw[i]->interactive = false;
            ui_widget_render(&amount_bw[i]->base);
          }
        }
      }
    }

    if (remaining_ms > 0 && client_state.hourglass_rotate_start != 0) {
      uint32_t timeout_ms = game_state->winner_declared ? game_settings->end_of_game_timeout_ms
                                                        : game_settings->action_timeout_ms;

      float fill_ratio = (float)remaining_ms / (float)timeout_ms;
      if (fill_ratio > 1.0f)
        fill_ratio = 1.0f;
      if (fill_ratio < 0.0f)
        fill_ratio = 0.0f;

      render_circle_timer(sdl_context->renderer, timer, fill_ratio);
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%" PRIu32, game_state->pot);
    render_text_pot(buffer, table_center, font);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);

    {
      int rmx, rmy;
      SDL_GetMouseState(&rmx, &rmy);
      float rlx, rly;
      SDL_RenderWindowToLogical(sdl_context->renderer, rmx, rmy, &rlx, &rly);
      mouse_pos.x = (int)rlx;
      mouse_pos.y = (int)rly;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
        DH_Card *card = &turn->hand.card[card_n];
        if (!DH_is_card_null(*card) || !DH_is_card_null(*card)) {
          card_context[my_id][card_n].base.hovered =
              SDL_PointInRect(&mouse_pos, &card_context[my_id][card_n].base.rect);
          if (card_context[my_id][card_n].base.hovered && event.type == SDL_MOUSEBUTTONDOWN &&
              client_state.do_discard_draw) {
            // select or deselect when clicked
            bool *selected = &card_context[my_id][card_n].base.selected;
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
          if (card_context[my_id][card_n].base.hovered)
            break;
        }
      }
      for (int i = 0; i < MAX_ACTIONS; i++) {
        action_bw[i]->base.hovered = SDL_PointInRect(&mouse_pos, &action_bw[i]->base.rect);
      }
      bool amount_selected = false;
      for (size_t i = 0; i < n_bet_amounts; i++) {
        if (!amount_bw[i]->interactive && amount_bw[i]->base.selected) {
          amount_bw[i]->base.selected = false;
          size_t next = (i + 1 < n_bet_amounts) ? i + 1 : 0;
          amount_bw[next]->base.selected = true;
          client_state.selected_amount = amount[next].value;
        }

        amount_bw[i]->base.hovered = SDL_PointInRect(&mouse_pos, &amount_bw[i]->base.rect);
        amount_selected =
            (amount_bw[i]->interactive &&
             ((amount_bw[i]->base.hovered && event.type == SDL_MOUSEBUTTONDOWN) ||
              (event.type == SDL_KEYDOWN && event.key.keysym.sym == amount_bw[i]->hotkey)));
        if (amount_selected) {
          if (!amount_bw[i]->base.selected) {
            for (size_t j = 0; j < n_bet_amounts; j++) {
              amount_bw[j]->base.selected = false;
            }
            amount_bw[i]->base.selected = true;
            amount_bw[i]->click.start_time = SDL_GetTicks();
            client_state.selected_amount = amount[i].value;
          }
          break;
        }
      }
      if (amount_selected)
        break;

      if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
          game_state->player[my_id].is_admin) {
        for (int8_t i = 0; i < MAX_PLAYERS; i++) {
          if (!game_nick_widgets[i] || !game_nick_widgets[i]->selectable)
            continue;
          if (SDL_PointInRect(&mouse_pos, &game_nick_widgets[i]->base.rect)) {
            if (selected_nick >= 0 && game_nick_widgets[selected_nick])
              game_nick_widgets[selected_nick]->base.selected = false;
            if (selected_nick == i)
              selected_nick = -1;
            else
              selected_nick = i;
            if (selected_nick >= 0)
              game_nick_widgets[selected_nick]->base.selected = true;
            break;
          }
        }
        if (selected_nick >= 0) {
          if (game_btn_kick->interactive &&
              SDL_PointInRect(&mouse_pos, &game_btn_kick->base.rect)) {
            send_kick_player(sock, selected_nick);
            selected_nick = -1;
          } else if (game_btn_ban->interactive &&
                     SDL_PointInRect(&mouse_pos, &game_btn_ban->base.rect)) {
            send_ban_player(sock, selected_nick);
            selected_nick = -1;
          }
        }
      }

      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_RETURN:
          if (event.key.keysym.mod & KMOD_ALT)
            toggle_fullscreen(sdl_context);
          break;

        case SDLK_F11:
          toggle_fullscreen(sdl_context);
          break;

        default:
          break;
        }
      }
      if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN) {
        if (my_turn && !client_state.do_discard_draw) {
          if (client_state.bet_check_fold || client_state.call_raise_fold ||
              client_state.call_complete_fold || client_state.complete_check_fold) {
            if (SDL_PointInRect(&mouse_pos, &action_bw[FOLD]->base.rect) ||
                event.key.keysym.sym == action_bw[FOLD]->hotkey) {
              verbose_puts("folding");
              if (send_player_action(&client_state, sock, ACTION_FOLD, 0) != 0)
                fprintf(stderr, "Failed to fold\n");
            }
          }
          if (client_state.bet_check_fold) {
            // TODO: use existing array (or modify it) to loop through each action
            if (SDL_PointInRect(&mouse_pos, &action_bw[BET]->base.rect) ||
                event.key.keysym.sym == action_bw[BET]->hotkey) {
              verbose_printf("betting %d\n", client_state.selected_amount);
              if (send_player_action(&client_state, sock, ACTION_BET,
                                     client_state.selected_amount) != 0)
                fprintf(stderr, "Failed to send bet\n");
            } else if (SDL_PointInRect(&mouse_pos, &action_bw[CHECK]->base.rect) ||
                       event.key.keysym.sym == action_bw[CHECK]->hotkey) {
              verbose_puts("checking");
              if (send_player_action(&client_state, sock, ACTION_CHECK, 0) != 0)
                fprintf(stderr, "Failed to check\n");
            }
          } else if (client_state.call_raise_fold) {
            if (action_bw[RAISE]->interactive &&
                (SDL_PointInRect(&mouse_pos, &action_bw[RAISE]->base.rect) ||
                 event.key.keysym.sym == action_bw[RAISE]->hotkey)) {
              if (send_player_action(&client_state, sock, ACTION_RAISE,
                                     client_state.selected_amount) == 0)
                verbose_printf("raising %d\n", client_state.selected_amount);
              else
                fputs("Failed to raise\n", stderr);
            } else if (SDL_PointInRect(&mouse_pos, &action_bw[CALL]->base.rect) ||
                       event.key.keysym.sym == action_bw[CALL]->hotkey) {
              verbose_puts("calling");
              if (send_player_action(&client_state, sock, ACTION_CALL, 0) != 0)
                fprintf(stderr, "Failed to call\n");
            }
          } else if (client_state.call_complete_fold) {
            if (action_bw[COMPLETE]->interactive &&
                (SDL_PointInRect(&mouse_pos, &action_bw[COMPLETE]->base.rect) ||
                 event.key.keysym.sym == action_bw[COMPLETE]->hotkey)) {
              if (send_player_action(&client_state, sock, ACTION_BET,
                                     client_state.selected_amount) == 0)
                verbose_printf("completing %d\n", client_state.selected_amount);
              else
                fputs("Failed to complete\n", stderr);
            } else if (SDL_PointInRect(&mouse_pos, &action_bw[CALL]->base.rect) ||
                       event.key.keysym.sym == action_bw[CALL]->hotkey) {
              verbose_puts("calling");
              if (send_player_action(&client_state, sock, ACTION_CALL, 0) != 0)
                fprintf(stderr, "Failed to call\n");
            }
          } else if (client_state.complete_check_fold) {
            if (action_bw[COMPLETE]->interactive &&
                (SDL_PointInRect(&mouse_pos, &action_bw[COMPLETE]->base.rect) ||
                 event.key.keysym.sym == action_bw[COMPLETE]->hotkey)) {
              if (send_player_action(&client_state, sock, ACTION_BET,
                                     client_state.selected_amount) == 0)
                verbose_printf("completing %d\n", client_state.selected_amount);
              else
                fputs("Failed to complete\n", stderr);
            } else if (SDL_PointInRect(&mouse_pos, &action_bw[CHECK]->base.rect) ||
                       event.key.keysym.sym == action_bw[CHECK]->hotkey) {
              verbose_puts("checking");
              if (send_player_action(&client_state, sock, ACTION_CHECK, 0) != 0)
                fprintf(stderr, "Failed to check\n");
            }
          }
        } else if (action_bw[DISCARD]->interactive && client_state.do_discard_draw &&
                   (SDL_PointInRect(&mouse_pos, &action_bw[DISCARD]->base.rect) ||
                    event.key.keysym.sym == action_bw[DISCARD]->hotkey)) {

          // Although the maximum allowed discards for 5 card draw can never
          // exceed 4, we need an array size of MAX_HAND_SIZE in case they select
          // all 5. However, the player will be required to have < 5 selected
          // to actually perform the discard.
          uint8_t discard_indices[MAX_HAND_SIZE] = {0};
          uint8_t discard_count = 0;

          for (uint8_t i = 0; i < MAX_HAND_SIZE; i++) {
            if (!card_context[my_id][i].base.selected)
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
        }
      }
    } // End Poll event
  }
  SDL_DestroyTexture(coin_tex_front);
  for (int i = 0; i < MAX_ACTIONS; i++)
    ui_widget_destroy(&action_bw[i]->base);
  for (size_t i = 0; i < n_bet_amounts; i++)
    ui_widget_destroy(&amount_bw[i]->base);
  ui_widget_destroy(&discard_hint_tw->base);
  for (int i = 0; i < SIZEOF_STATUS_MSGS; i++)
    ui_widget_destroy(&status_tw[i]->base);
  ui_destroy_all(&registry);
  return running;
}

typedef struct {
  IPaddress server_ip;
  TCPsocket sock;
  SDL_atomic_t done;
} ConnectAttempt_t;

static int connect_thread_fn(void *data) {
  ConnectAttempt_t *ca = data;
  ca->sock = SDLNet_TCP_Open(&ca->server_ip);
  SDL_AtomicSet(&ca->done, 1);
  return 0;
}

int authenticate_with_server(TCPsocket sock, const char *password) {
  unsigned char nonce[NONCE_SIZE];
  unsigned char hash[HASH_SIZE];

  /* receive nonce */
  if (recv_all_tcp(sock, nonce, NONCE_SIZE) < 0) {
    fprintf(stderr, "Failed to receive nonce\n");
    return -1;
  }

  /* compute SHA256(password + nonce) */
#ifdef HAVE_LIBSODIUM
  crypto_hash_sha256_state state;

  crypto_hash_sha256_init(&state);
  crypto_hash_sha256_update(&state, (const unsigned char *)password, strlen(password));
  crypto_hash_sha256_update(&state, nonce, NONCE_SIZE);
  crypto_hash_sha256_final(&state, hash);
#else
  (void)password;
  (void)nonce;
  memset(hash, 0, HASH_SIZE);
#endif

  /* send response */
  if (send_all_tcp(sock, hash, HASH_SIZE) != 0) {
    fprintf(stderr, "Failed to send authentication response\n");
    return -1;
  }

  return 0;
}

bool get_socket_context_and_run_client(PlayerConfig_t *player_config, const CliArgs_t *cli_args,
                                       const char *host_str, const uint16_t port,
                                       SdlContext_t *sdl_context, Font_t *font, Path_t *path,
                                       const bool test_mode, LinkWidget_t **links,
                                       SocketContext_t *out_socket_context) {
  IPaddress server_ip;
  SocketContext_t socket_context = {0};

  if (SDLNet_ResolveHost(&server_ip, host_str, port) == -1) {
    fprintf(stderr, "Failed to resolve server: %s\n", SDLNet_GetError());
    return false;
  }

  // SDLNet_TCP_Open blocks for the OS TCP timeout on unreachable hosts.
  // Run each attempt on a background thread; heap-allocate the state so we
  // can safely SDL_DetachThread (rather than WaitThread) on cancel/timeout.
  // Per-attempt timeout keeps the counter advancing on slow/unreachable hosts.
  static const Uint32 ATTEMPT_TIMEOUT_MS = 5000;
  static const Uint32 RETRY_DELAY_MS = 2000;

  ButtonWidget_t *btn_cancel = NULL;
  TextWidget_t *status_tw = NULL;
  if (sdl_context && font) {
    btn_cancel = button_widget_create(_("Cancel"), (EColor_t){COLOR_BLACK, COLOR_YELLOW},
                                      font->fonts[FONT_BOLD], SDLK_ESCAPE);
    if (btn_cancel) {
      btn_cancel->base.rect.x = g_center.x - btn_cancel->base.rect.w / 2;
      btn_cancel->base.rect.y = g_center.y + 60;
    }
    char initial_status[256] = {0};
    snprintf(initial_status, sizeof(initial_status), _("Attempting connection to: %s... (%d/%d)"),
             host_str, 1, player_config->connect_attempts);
    status_tw =
        text_widget_create(initial_status, font->fonts[FONT_DEFAULT], get_color(COLOR_WHITE));
    if (status_tw) {
      status_tw->base.rect.x = 10;
      status_tw->base.rect.y = g_center.y;
    }
  }

  bool cancelled = false;
  bool sdl_quit = false;
  uint8_t attempts;
  for (attempts = 0; attempts < player_config->connect_attempts; ++attempts) {
    if (status_tw && attempts > 0) {
      char tmp[256] = {0};
      snprintf(tmp, sizeof(tmp), _("Attempting connection to: %s... (%d/%d)"), host_str,
               attempts + 1, player_config->connect_attempts);
      text_widget_set_text(status_tw, tmp);
      status_tw->base.rect.x = 10;
      status_tw->base.rect.y = g_center.y;
    }

    ConnectAttempt_t *ca = SDL_malloc(sizeof(ConnectAttempt_t));
    if (!ca)
      break;
    ca->server_ip = server_ip;
    ca->sock = NULL;
    SDL_AtomicSet(&ca->done, 0);

    SDL_Thread *thread = SDL_CreateThread(connect_thread_fn, "tcp_connect", ca);
    if (!thread) {
      // Fallback: blocking connect with no event handling this attempt
      ca->sock = SDLNet_TCP_Open(&server_ip);
      SDL_AtomicSet(&ca->done, 1);
    }

    Uint32 attempt_start = SDL_GetTicks();
    bool timed_out = false;

    while (!SDL_AtomicGet(&ca->done) && !cancelled && !sdl_quit) {
      if (SDL_GetTicks() - attempt_start >= ATTEMPT_TIMEOUT_MS) {
        timed_out = true;
        break;
      }
      if (sdl_context) {
        clear_screen(sdl_context->renderer);
        if (status_tw)
          status_tw->base.render(&status_tw->base);
        if (btn_cancel)
          ui_widget_render(&btn_cancel->base);
        SDL_RenderPresent(sdl_context->renderer);
      }
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        SDL_Point mp = {e.button.x, e.button.y};
        if (e.type == SDL_QUIT) {
          sdl_quit = true;
        } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
          cancelled = true;
        } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
          if (btn_cancel && SDL_PointInRect(&mp, &btn_cancel->base.rect))
            cancelled = true;
        } else if (e.type == SDL_MOUSEMOTION) {
          SDL_Point mmp = {e.motion.x, e.motion.y};
          if (btn_cancel)
            btn_cancel->base.hovered = SDL_PointInRect(&mmp, &btn_cancel->base.rect);
        }
      }
      SDL_Delay(16);
    }

    if (thread && !SDL_AtomicGet(&ca->done)) {
      // Thread is still running (cancelled or timed out). Detach it and leave
      // ca allocated — the thread will write to it and exit on its own.
      SDL_DetachThread(thread);
      thread = NULL;
    } else {
      // Thread finished normally; safe to wait and free.
      if (thread)
        SDL_WaitThread(thread, NULL);
      TCPsocket s = ca->sock;
      SDL_free(ca);
      ca = NULL;
      if (s) {
        socket_context.sock = s;
        break;
      }
    }

    if (cancelled || sdl_quit)
      break;

    if (!timed_out)
      fprintf(stderr, "Attempt %d: Failed to connect to server: %s\n", attempts + 1,
              SDLNet_GetError());

    // Brief pause between retries (skip if this was the last attempt)
    if (attempts < (uint8_t)(player_config->connect_attempts - 1)) {
      Uint32 start = SDL_GetTicks();
      while (SDL_GetTicks() - start < RETRY_DELAY_MS && !cancelled && !sdl_quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          SDL_Point mp = {e.button.x, e.button.y};
          if (e.type == SDL_QUIT) {
            sdl_quit = true;
          } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            cancelled = true;
          } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            if (btn_cancel && SDL_PointInRect(&mp, &btn_cancel->base.rect))
              cancelled = true;
          } else if (e.type == SDL_MOUSEMOTION) {
            SDL_Point mmp = {e.motion.x, e.motion.y};
            if (btn_cancel)
              btn_cancel->base.hovered = SDL_PointInRect(&mmp, &btn_cancel->base.rect);
          }
        }
        SDL_Delay(16);
      }
    }
  }

  if (btn_cancel)
    ui_widget_destroy(&btn_cancel->base);
  if (status_tw)
    ui_widget_destroy(&status_tw->base);

  if (!socket_context.sock) {
    if (!cancelled && !sdl_quit)
      printf("All %d attempts failed.\n", attempts);
    return !sdl_quit;
  }

  TCPsocket sock = socket_context.sock;
  socket_context.set = SDLNet_AllocSocketSet(1);
  if (!socket_context.set) {
    fprintf(stderr, "Failed to allocate socket set: %s\n", SDLNet_GetError());
    SDLNet_TCP_Close(sock);
    return false;
  }

  if (SDLNet_TCP_AddSocket(socket_context.set, sock) == -1)
    fputs("Socket set full\n", stderr);

  if (send_protocol_header(sock) != 0)
    goto cleanup;

  if (!test_mode) {
    const char *env_pw = getenv("DC_PASSWORD");
    const char *password = env_pw ? env_pw : player_config->password;
    if (authenticate_with_server(sock, password) < 0)
      fprintf(stderr, "Authentication attempt failed\n");

    GameState_t game_state = {0};
    GameSettings_t game_settings = {0};
    ClientState_t client_state = {0};
    char *nick = player_config->nick;
    uint16_t len = (uint16_t)(strlen(nick) + 1);
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

    bool went_back_result = false;
    atomic_store(&g_audio_needs_restart, false);
    atomic_store(&g_audio_shutting_down, false);
    SoundContext_t sound_context = {0};
    sound_context.engineConfig = ma_engine_config_init();
    if (player_config->volume == 0 || cli_args->disable_audio) {
      sound_context.engineConfig.noDevice = MA_TRUE;
      sound_context.engineConfig.channels = 2;
      sound_context.engineConfig.sampleRate = 48000;
    } else {
      verbose_puts("Initializing audio engine (powered by miniaudio: https://miniaud.io/)");
      sound_context.engineConfig.notificationCallback = on_audio_device_notification;
    }
    sound_context.result = ma_engine_init(&sound_context.engineConfig, &sound_context.engine);
    if (sound_context.result != MA_SUCCESS) {
      fprintf(stderr, "Error: Failed to initialize miniaudio engine (code: %d).\n",
              sound_context.result);
      exit(EXIT_FAILURE);
    }
    if (!sound_context.engineConfig.noDevice)
      g_sound_context = &sound_context;
    ma_engine_set_volume(&sound_context.engine, player_config->volume * .1f);

    // Using {0} or {{0}} for the The ma_sound field initializer doesn't work so
    // using 'ma_tmp' instead
    ma_sound ma_tmp = {0};
    Sound_t sounds[] = {[SND_SERVER_JOIN] = {"server_join.wav", ma_tmp},
                        [SND_MY_TURN] = {"my_turn.wav", ma_tmp},
                        [SND_GAME_OVER] = {"game_over.wav", ma_tmp}};

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
    size_t n_sounds_init = 0;
    size_t n_coin_sounds_init = 0;
    for (i = 0; i < SND_NUM_SOUNDS; i++) {
      char *snd_path = join_paths(limits.path_max, path->data, "sounds", sounds[i].filename);
      bool ok = ma_sound_init_from_file(&sound_context.engine, snd_path, 0, NULL, NULL,
                                        &sounds[i].sound) == MA_SUCCESS;
      free(snd_path);
      if (!ok) {
        fprintf(stderr, "Failed to init sound %zd\n", i);
        goto cleanup_audio;
      }
      n_sounds_init++;
    }

    for (i = 0; i < ARRAY_SIZE(coin_hit_sounds); i++) {
      char *snd_path =
          join_paths(limits.path_max, path->data, "sounds/coin", coin_hit_sounds[i].filename);
      bool ok = ma_sound_init_from_file(&sound_context.engine, snd_path, 0, NULL, NULL,
                                        &coin_hit_sounds[i].sound) == MA_SUCCESS;
      free(snd_path);
      if (!ok) {
        fprintf(stderr, "Failed to init sound %zd\n", i);
        goto cleanup_audio;
      }
      n_coin_sounds_init++;
    }

    {
      bool running = true;
      bool went_back = false;
      do {
        if (atomic_exchange(&g_audio_needs_restart, false)) {
          if (ma_engine_start(&sound_context.engine) != MA_SUCCESS)
            fputs("Warning: failed to restart audio engine after device change\n", stderr);
          else
            verbose_puts("Audio engine restarted after device change");
        }
        EGameSelResult_t sel = handle_game_selection(
            player_config, &socket_context, game_settings.client_id, &game_state, &client_state,
            sdl_context, font, &sound_context, links, path);
        if (sel == GAME_SEL_BACK) {
          went_back = true;
          break;
        }
        if (sel == GAME_SEL_ERROR)
          break;

        running = handle_game_logic(player_config, &socket_context, &game_settings, &game_state,
                                    sdl_context, font, path, &sound_context);
      } while (running);
      went_back_result = went_back;
    }
  cleanup_audio:
    for (i = 0; i < n_sounds_init; i++)
      ma_sound_uninit(&sounds[i].sound);
    for (i = 0; i < n_coin_sounds_init; i++)
      ma_sound_uninit(&coin_hit_sounds[i].sound);
    atomic_store(&g_audio_shutting_down, true);
    g_sound_context = NULL;
    ma_engine_uninit(&sound_context.engine);
    socket_cleanup(&socket_context);
    SDLNet_Quit();

    return went_back_result;
  } else {
    if (out_socket_context)
      *out_socket_context = socket_context;
    return false;
  }

cleanup:
  socket_cleanup(&socket_context);
  SDLNet_Quit();
  return false;
}

void do_sdl_cleanup(SdlContext_t *sdl_context) {
  SDL_DestroyRenderer(sdl_context->renderer);
  SDL_DestroyWindow(sdl_context->window);
  SDL_Quit();
}
