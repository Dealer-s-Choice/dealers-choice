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

#include <canfigger.h>
#ifdef _WIN32
#include "dc_windows.h"
#else
#include <dirent.h>
#endif
#include <math.h>
#include <stdatomic.h>

#include <deckhandler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"
#include "client_internal.h"
#include "game.h"
#include "globals_gui.h"
#include "graphics.h"
#include "hotkeys.h"
#include "widgets/button.h"
#include "widgets/card_text_atlas.h"
#include "widgets/dealer.h"
#include "widgets/image.h"
#include "widgets/indicator.h"
#include "widgets/nick.h"
#include "widgets/ping.h"
#include "widgets/step_scale.h"
#include "widgets/text.h"

#include "util.h"

#include <sodium.h>

// Build fails using gcc on Ubuntu 24.04 (and maybe others) without this
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// What's the max this needs to be to support the unicode suit symbol?
// SIZEOF_CARD_TEXT is defined in client.h

#define AUDIO_SLOW_WARN_MS 500 /* audio engine init/uninit blocking >= this (#307) */

#define MAX_COIN_IMAGES 16

void ma_sound_start_wrap(ma_sound *pSound, const char *file, const int line) {
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

void detect_player_changes(const GameState_t *gs, bool was_connected[MAX_PLAYERS],
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
  GAME_SEL_QUIT = 3,
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
      button_widget_create_styled(_("Deuces Wild"), &ROLE_ALT, font->fonts, (SDL_Keycode)0);

  bool dealing = true;

  layout_links(links, LINK_DEFS_COUNT);

  static uint8_t saved_n_clients = 0;
  tcpme_socket_t sock = socket_context->sock;

  UIRegistry_t registry = {0};
  ui_register(&registry, &button_deuces_wild->base);

  for (int i = 0; i < MAX_CHOICES; i++) {
    game_choice_button[i] = button_widget_create_styled(game_choices[i].str, &ROLE_PRIMARY,
                                                        font->fonts, (SDL_Keycode)0);
    ui_register(&registry, &game_choice_button[i]->base);
  }

  UITable_t gc_table = {0};
  ui_table_begin(&gc_table, 0, g_viewport.y + g_layout_cfg.margin, 2);
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
  button_deuces_wild->base.rect.y = gc_bottom + g_layout_cfg.margin;

  NickWidget_t *nick_widgets[MAX_PLAYERS] = {0};
  DealerWidget_t *dealer_widgets[MAX_PLAYERS] = {0};
  int8_t selected_nick = -1;
  PingWidget_t *ping_widgets[MAX_PLAYERS] = {0};
  UITable_t table = {0};
  bool table_needs_rebuild = true;

  TextWidget_t *connected_tw =
      text_widget_create(_("Players"), font->fonts[FONT_BOLD], DC_TEXT_ON_LIGHT);
  ui_register(&registry, &connected_tw->base);

  TextWidget_t *dealer_label_tw =
      text_widget_create(_("Dealer"), font->fonts[FONT_BOLD], DC_TEXT_ON_LIGHT);
  ui_register(&registry, &dealer_label_tw->base);

  TextWidget_t *ping_label_tw =
      text_widget_create(_("Ping"), font->fonts[FONT_BOLD], DC_TEXT_ON_LIGHT);
  ui_register(&registry, &ping_label_tw->base);

  char version_str[64] = {0};
  snprintf(version_str, sizeof(version_str), "Version " DEALERSCHOICE_VERSION);
  TextWidget_t *tw_version_lobby =
      text_widget_create(version_str, font->fonts[FONT_VERSION], DC_TEXT_ON_DARK);
  if (tw_version_lobby) {
    ui_widget_place(&tw_version_lobby->base, g_viewport.x + g_layout_cfg.margin,
                    g_viewport.y + g_viewport.h - tw_version_lobby->base.rect.h -
                        g_layout_cfg.margin);
    ui_register(&registry, &tw_version_lobby->base);
  }

  TextWidget_t *waiting_players_tw = text_widget_create(_("Waiting for more players..."),
                                                        font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
  ui_register(&registry, &waiting_players_tw->base);
  waiting_players_tw->base.rect.x = g_center.x;
  waiting_players_tw->base.rect.y = g_layout.lobby.waiting_y;

  TextWidget_t *waiting_dealer_tw = text_widget_create(_("Waiting for dealer to select game..."),
                                                       font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
  ui_register(&registry, &waiting_dealer_tw->base);
  waiting_dealer_tw->base.rect.x = g_center.x;
  waiting_dealer_tw->base.rect.y = g_layout.lobby.waiting_y;

  ButtonWidget_t *btn_kick =
      button_widget_create_styled(_("Kick"), &ROLE_ALT, font->fonts, (SDL_Keycode)0);
  ButtonWidget_t *btn_ban =
      button_widget_create_styled(_("Ban"), &ROLE_ALT, font->fonts, (SDL_Keycode)0);
  btn_kick->base.rect.x = g_layout.lobby.kick_x;
  btn_kick->base.rect.y = g_layout.lobby.kick_y;
  btn_ban->base.rect.x =
      btn_kick->base.rect.x + btn_kick->base.rect.w + g_layout_cfg.kick_ban_btn_gap;
  btn_ban->base.rect.y = g_layout.lobby.kick_y;
  ui_register(&registry, &btn_kick->base);
  ui_register(&registry, &btn_ban->base);

  ButtonWidget_t *btn_quit_lobby =
      button_widget_create_styled("X", &ROLE_DANGER, font->fonts, (SDL_Keycode)0);
  if (btn_quit_lobby) {
    btn_quit_lobby->base.rect.x =
        g_viewport.x + g_viewport.w - btn_quit_lobby->base.rect.w - g_layout_cfg.margin;
    btn_quit_lobby->base.rect.y = g_layout.menu.quit_y;
    ui_register(&registry, &btn_quit_lobby->base);
  }

  static bool was_connected[MAX_PLAYERS] = {0};

  EGameSelResult_t result = GAME_SEL_SUCCESS;

  char *back_img_path = canfigger_path_join(path->data, "images/arrow_back.png");
  ImageWidget_t *back_img = back_img_path
                                ? image_widget_create(back_img_path, g_layout_cfg.back_btn_size,
                                                      g_layout_cfg.back_btn_size)
                                : NULL;
  free(back_img_path);
  if (back_img) {
    back_img->base.rect.x = g_layout.menu.back_img_x;
    back_img->base.rect.y = g_layout.menu.back_img_y;
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
      int end_y = g_viewport.y + g_viewport.h - g_layout_cfg.back_btn_size - 20;
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
    if (btn_quit_lobby)
      btn_quit_lobby->base.hovered = SDL_PointInRect(&mouse_pos, &btn_quit_lobby->base.rect);
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
        SDL_PushEvent(&e);
        result = GAME_SEL_QUIT;
        goto cleanup;

      case SDL_MOUSEBUTTONDOWN: {
        if (e.button.button == SDL_BUTTON_LEFT) {
          if (btn_quit_lobby && SDL_PointInRect(&mouse_pos, &btn_quit_lobby->base.rect) &&
              confirm_quit(font->fonts)) {
            SDL_Event quit = {.type = SDL_QUIT};
            SDL_PushEvent(&quit);
            result = GAME_SEL_QUIT;
            goto cleanup;
          }
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

        case SDLK_ESCAPE:
          if (confirm_quit(font->fonts)) {
            SDL_Event quit = {.type = SDL_QUIT};
            SDL_PushEvent(&quit);
            result = GAME_SEL_QUIT;
            goto cleanup;
          }
          break;

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

SDL_Texture *load_coin_texture(SDL_Renderer *renderer, const char *base_path, const char *coin) {
  const char *subdir = "/images/";
  size_t len = strlen(base_path) + strlen(subdir) + strlen(coin) + 1;

  char *full_path = calloc_wrap(len, 1);
  snprintf(full_path, len, "%s%s%s", base_path, subdir, coin);

  SDL_Texture *tex = load_texture(renderer, full_path);
  free(full_path);
  return tex;
}

/* Reads {base_path}/images/coins/, loads up to max_count .png files as
 * textures into out[]. Returns the number loaded. Warns if capped. */
static size_t load_coin_textures(SDL_Renderer *renderer, const char *base_path, SDL_Texture **out,
                                 size_t max_count) {
  const char *suffix = "/images/coins";
  char *dirpath = calloc_wrap(strlen(base_path) + strlen(suffix) + 1, 1);
  snprintf(dirpath, strlen(base_path) + strlen(suffix) + 1, "%s%s", base_path, suffix);

  size_t i = 0;

#ifdef _WIN32
  char pattern[512];
  snprintf(pattern, sizeof pattern, "%s\\*.png", dirpath);
  free(dirpath);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  do {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      continue;
    if (i >= max_count) {
      fprintf(stderr, "Warning: more than %zu coin images found; increase MAX_COIN_IMAGES\n",
              max_count);
      break;
    }
    char rel[512];
    snprintf(rel, sizeof rel, "coins/%s", fd.cFileName);
    out[i++] = load_coin_texture(renderer, base_path, rel);
  } while (FindNextFileA(h, &fd));
  FindClose(h);
#else
  DIR *d = opendir(dirpath);
  free(dirpath);
  if (!d)
    return 0;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    size_t nlen = strlen(ent->d_name);
    if (nlen <= 4 || strcmp(ent->d_name + nlen - 4, ".png") != 0)
      continue;
    if (i >= max_count) {
      fprintf(stderr, "Warning: more than %zu coin images found; increase MAX_COIN_IMAGES\n",
              max_count);
      break;
    }
    char rel[512];
    snprintf(rel, sizeof rel, "coins/%s", ent->d_name);
    out[i++] = load_coin_texture(renderer, base_path, rel);
  }
  closedir(d);
#endif

  return i;
}

typedef struct {
  const char *host_str;
  uint16_t port;
  tcpme_socket_t sock;
  SDL_atomic_t done;
  SDL_atomic_t orphaned;
} ConnectAttempt_t;

static int connect_thread_fn(void *data) {
  ConnectAttempt_t *ca = data;
  ca->sock = tcpme_connect(ca->host_str, ca->port);
  if (tcpme_socket_valid(ca->sock))
    tcpme_set_timeout(ca->sock, SOCKET_IO_TIMEOUT_MS);
  SDL_AtomicSet(&ca->done, 1);
  if (SDL_AtomicGet(&ca->orphaned)) {
    if (tcpme_socket_valid(ca->sock))
      tcpme_close(ca->sock);
    SDL_free(ca);
  }
  return 0;
}

typedef struct {
  ma_engine_config engineConfig;
  ma_engine *engine;
  ma_result result;
  SDL_atomic_t done;
} AudioInitAttempt_t;

static int audio_init_thread_fn(void *data) {
  AudioInitAttempt_t *aa = data;
  uint32_t t0 = SDL_GetTicks();
  aa->result = ma_engine_init(&aa->engineConfig, aa->engine);
  uint32_t took = SDL_GetTicks() - t0;
  if (took >= AUDIO_SLOW_WARN_MS)
    dc_log(DC_LOG_WARN, "audio engine init took %ums (slow audio backend)", took);
  SDL_AtomicSet(&aa->done, 1);
  return 0;
}

typedef struct {
  ma_engine *engine;
  SDL_atomic_t done;
} AudioUninitAttempt_t;

static int audio_uninit_thread_fn(void *data) {
  AudioUninitAttempt_t *au = data;
  uint32_t t0 = SDL_GetTicks();
  ma_engine_uninit(au->engine);
  uint32_t took = SDL_GetTicks() - t0;
  if (took >= AUDIO_SLOW_WARN_MS)
    dc_log(DC_LOG_WARN, "audio engine uninit took %ums (slow audio backend)", took);
  SDL_AtomicSet(&au->done, 1);
  return 0;
}

bool get_socket_context_and_run_client(PlayerConfig_t *player_config, const CliArgs_t *cli_args,
                                       const char *host_str, const uint16_t port,
                                       SdlContext_t *sdl_context, Font_t *font, Path_t *path,
                                       LinkWidget_t **links, SocketContext_t *out_socket_context) {
  SocketContext_t socket_context = {0};

  // tcpme_connect blocks for the OS TCP timeout on unreachable hosts.
  // Run each attempt on a background thread; heap-allocate the state so we
  // can safely SDL_DetachThread (rather than WaitThread) on cancel/timeout.
  // Per-attempt timeout keeps the counter advancing on slow/unreachable hosts.
  static const Uint32 ATTEMPT_TIMEOUT_MS = 5000;

  ButtonWidget_t *btn_cancel = NULL;
  TextWidget_t *status_tw = NULL;
  if (sdl_context && font) {
    btn_cancel = button_widget_create_styled(_("Cancel"), &ROLE_PRIMARY, font->fonts, SDLK_ESCAPE);
    if (btn_cancel) {
      btn_cancel->base.rect.x = g_center.x - btn_cancel->base.rect.w / 2;
      btn_cancel->base.rect.y = g_center.y + 60;
    }
    char initial_status[256] = {0};
    snprintf(initial_status, sizeof(initial_status), _("Attempting connection to: %s... (%d/%d)"),
             host_str, 1, player_config->connect_attempts);
    status_tw = text_widget_create(initial_status, font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
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
    ca->host_str = host_str;
    ca->port = port;
    ca->sock = TCPME_INVALID_SOCKET;
    SDL_AtomicSet(&ca->done, 0);
    SDL_AtomicSet(&ca->orphaned, 0);

    SDL_Thread *thread = SDL_CreateThread(connect_thread_fn, "tcp_connect", ca);
    if (!thread) {
      // Fallback: blocking connect with no event handling this attempt
      ca->sock = tcpme_connect(host_str, port);
      if (tcpme_socket_valid(ca->sock))
        tcpme_set_timeout(ca->sock, SOCKET_IO_TIMEOUT_MS);
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
      // Thread is still running (cancelled or timed out). Mark it orphaned so
      // the thread closes any socket it opens and frees ca itself.
      SDL_AtomicSet(&ca->orphaned, 1);
      SDL_DetachThread(thread);
      thread = NULL;
    } else {
      // Thread finished normally; safe to wait and free.
      if (thread)
        SDL_WaitThread(thread, NULL);
      tcpme_socket_t s = ca->sock;
      SDL_free(ca);
      ca = NULL;
      if (tcpme_socket_valid(s)) {
        socket_context.sock = s;
        break;
      }
    }

    if (cancelled || sdl_quit)
      break;

    if (!timed_out)
      fprintf(stderr, "Attempt %d: Failed to connect to server: %s\n", attempts + 1,
              tcpme_get_error());

    // Wait out the remainder of ATTEMPT_TIMEOUT_MS so each attempt cycle
    // takes the full 5 seconds even when the connect fails immediately
    // (e.g. ECONNREFUSED). Skip on last attempt.
    if (attempts < (uint8_t)(player_config->connect_attempts - 1)) {
      Uint32 elapsed = SDL_GetTicks() - attempt_start;
      Uint32 wait_ms = elapsed < ATTEMPT_TIMEOUT_MS ? ATTEMPT_TIMEOUT_MS - elapsed : 0;
      Uint32 start = SDL_GetTicks();
      while (SDL_GetTicks() - start < wait_ms && !cancelled && !sdl_quit) {
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

  if (!tcpme_socket_valid(socket_context.sock)) {
    if (!cancelled && !sdl_quit)
      printf("All %d attempts failed.\n", attempts);
    return !sdl_quit;
  }

  tcpme_socket_t sock = socket_context.sock;
  socket_context.set = tcpme_alloc_set(1);
  if (!socket_context.set) {
    fprintf(stderr, "Failed to allocate socket set: %s\n", tcpme_get_error());
    tcpme_close(sock);
    return false;
  }

  if (tcpme_add_socket(socket_context.set, sock) != 0)
    fputs("Socket set full\n", stderr);

  if (send_protocol_header(sock, 0) != 0)
    goto cleanup;

  if (!dc_test_mode) {
    const char *env_pw = getenv("DC_PASSWORD");
    const char *password = env_pw ? env_pw : player_config->password;
    if (authenticate_with_server(sock, password) < 0)
      fprintf(stderr, "Authentication attempt failed\n");

    GameState_t game_state = {0};
    GameSettings_t game_settings = {0};
    ClientState_t client_state = {0};
    char *nick = player_config->nick;
    uint16_t len = (uint16_t)strlen(nick);
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
    bool audio_sdl_quit = false;
    size_t i;
    size_t n_sounds_init = 0;
    size_t n_coin_sounds_init = 0;
    size_t n_coin_images = 0;
    SDL_Texture *coin_textures[MAX_COIN_IMAGES] = {NULL};
    atomic_store(&g_audio_needs_restart, false);
    atomic_store(&g_audio_shutting_down, false);
    SoundContext_t sound_context = {0};
    sound_context.engineConfig = ma_engine_config_init();
    if (player_config->volume == 0 || cli_args->disable_audio) {
      sound_context.engineConfig.noDevice = MA_TRUE;
      sound_context.engineConfig.channels = 2;
      sound_context.engineConfig.sampleRate = 48000;
      sound_context.result = ma_engine_init(&sound_context.engineConfig, &sound_context.engine);
    } else {
      // ma_engine_init can block for seconds when a sound server (e.g. PulseAudio)
      // is unreachable. Run it on a background thread so the window stays responsive.
      verbose_puts("Initializing audio engine (powered by miniaudio: https://miniaud.io/)");
      sound_context.engineConfig.notificationCallback = on_audio_device_notification;
      AudioInitAttempt_t aa = {.engineConfig = sound_context.engineConfig,
                               .engine = &sound_context.engine};
      SDL_AtomicSet(&aa.done, 0);
      SDL_Thread *audio_thread = SDL_CreateThread(audio_init_thread_fn, "audio_init", &aa);
      if (!audio_thread) {
        aa.result = ma_engine_init(&aa.engineConfig, aa.engine);
        SDL_AtomicSet(&aa.done, 1);
      }
      while (!SDL_AtomicGet(&aa.done)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          if (e.type == SDL_QUIT)
            audio_sdl_quit = true;
        }
        SDL_Delay(16);
      }
      if (audio_thread)
        SDL_WaitThread(audio_thread, NULL);
      sound_context.result = aa.result;
      if (sound_context.result != MA_SUCCESS) {
        fprintf(
            stderr,
            "Warning: Failed to initialize audio engine (code: %d), continuing without audio.\n",
            sound_context.result);
        sound_context.engineConfig.noDevice = MA_TRUE;
        sound_context.engineConfig.channels = 2;
        sound_context.engineConfig.sampleRate = 48000;
        sound_context.engineConfig.notificationCallback = NULL;
        sound_context.result = ma_engine_init(&sound_context.engineConfig, &sound_context.engine);
      }
    }
    if (sound_context.result != MA_SUCCESS) {
      fprintf(stderr, "Error: Failed to initialize audio engine (code: %d).\n",
              sound_context.result);
      exit(EXIT_FAILURE);
    }
    if (audio_sdl_quit)
      goto cleanup_audio;
    if (!sound_context.engineConfig.noDevice)
      g_sound_context = &sound_context;
    ma_engine_set_volume(&sound_context.engine, player_config->volume * .1f);

    // Using {0} or {{0}} for the The ma_sound field initializer doesn't work so
    // using 'ma_tmp' instead
    ma_sound ma_tmp = {0};
    Sound_t sounds[] = {[SND_SERVER_JOIN] = {"server_join.wav", ma_tmp},
                        [SND_MY_TURN] = {"my_turn.wav", ma_tmp},
                        [SND_MY_TURN_LAST_CHANCE] = {"my_turn_last_chance.wav", ma_tmp},
                        [SND_GAME_OVER] = {"game_over.wav", ma_tmp}};

    Sound_t coin_hit_sounds[] = {
        {"coin_hit_001.wav", ma_tmp}, {"coin_hit_002.wav", ma_tmp}, {"coin_hit_003.wav", ma_tmp},
        {"coin_hit_004.wav", ma_tmp}, {"coin_hit_005.wav", ma_tmp}, {"coin_hit_006.wav", ma_tmp},
        {"coin_hit_007.wav", ma_tmp},
    };

    sound_context.coin_array_size = ARRAY_SIZE(coin_hit_sounds);

    sound_context.sounds = sounds;
    sound_context.coin_hit_sounds = coin_hit_sounds;
    for (i = 0; i < SND_NUM_SOUNDS; i++) {
      char *sub = canfigger_path_join("sounds", sounds[i].filename);
      char *snd_path = canfigger_path_join(path->data, sub);
      free(sub);
      if (!snd_path) {
        fprintf(stderr, "Failed to build sound path %zd\n", i);
        goto cleanup_audio;
      }
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
      char *sub = canfigger_path_join("sounds/coin", coin_hit_sounds[i].filename);
      char *snd_path = canfigger_path_join(path->data, sub);
      free(sub);
      if (!snd_path) {
        fprintf(stderr, "Failed to build sound path %zd\n", i);
        goto cleanup_audio;
      }
      bool ok = ma_sound_init_from_file(&sound_context.engine, snd_path, 0, NULL, NULL,
                                        &coin_hit_sounds[i].sound) == MA_SUCCESS;
      free(snd_path);
      if (!ok) {
        fprintf(stderr, "Failed to init sound %zd\n", i);
        goto cleanup_audio;
      }
      n_coin_sounds_init++;
    }

    n_coin_images =
        load_coin_textures(sdl_context->renderer, path->data, coin_textures, MAX_COIN_IMAGES);

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
        if (sel == GAME_SEL_BACK || sel == GAME_SEL_ERROR || sel == GAME_SEL_QUIT) {
          went_back = true;
          break;
        }

        EGameLogicResult_t result = handle_game_logic(
            player_config, &socket_context, &game_settings, &game_state, sdl_context, font, path,
            &sound_context, coin_textures, n_coin_images);
        if (result == GAME_LOGIC_DISCONNECTED)
          went_back = true;
        running = (result == GAME_LOGIC_AT_MENU);
      } while (running);
      went_back_result = went_back;
    }
  cleanup_audio:
    for (i = 0; i < n_sounds_init; i++)
      ma_sound_uninit(&sounds[i].sound);
    for (i = 0; i < n_coin_sounds_init; i++)
      ma_sound_uninit(&coin_hit_sounds[i].sound);
    for (i = 0; i < n_coin_images; i++)
      SDL_DestroyTexture(coin_textures[i]);
    atomic_store(&g_audio_shutting_down, true);
    g_sound_context = NULL;
    {
      AudioUninitAttempt_t au;
      au.engine = &sound_context.engine;
      SDL_AtomicSet(&au.done, 0);
      SDL_Thread *uninit_thread = SDL_CreateThread(audio_uninit_thread_fn, "audio_uninit", &au);
      if (!uninit_thread) {
        ma_engine_uninit(&sound_context.engine);
      } else {
        while (!SDL_AtomicGet(&au.done)) {
          SDL_PumpEvents();
          SDL_Delay(16);
        }
        SDL_WaitThread(uninit_thread, NULL);
      }
    }
    socket_cleanup(&socket_context);

    return went_back_result;
  } else {
    if (out_socket_context)
      *out_socket_context = socket_context;
    return false;
  }

cleanup:
  socket_cleanup(&socket_context);
  return false;
}

void do_sdl_cleanup(SdlContext_t *sdl_context) {
  /* Atlas textures are tied to sdl_context->renderer — must be freed
   * before the renderer they were created with. */
  card_text_atlas_destroy();
  SDL_DestroyRenderer(sdl_context->renderer);
  SDL_DestroyWindow(sdl_context->window);
  SDL_Quit();
}
