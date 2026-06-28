/*
 game_select.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2026 Andy Alt

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

/* The pre-game lobby / game-selection screen (handle_game_selection).  Split
 * out of client.c; cross-file helpers shared with client.c are declared in
 * client_internal.h. */

#include <canfigger.h>
#include <stdio.h>
#include <string.h>

#include "client.h"
#include "client_internal.h"
#include "game.h"
#include "globals_gui.h"
#include "graphics.h"
#include "hotkey_overlay.h"
#include "hotkeys.h"
#include "widgets/button.h"
#include "widgets/dealer.h"
#include "widgets/image.h"
#include "widgets/nick.h"
#include "widgets/ping.h"
#include "widgets/text.h"

#include "util.h"

EGameSelResult_t handle_game_selection(const PlayerConfig_t *player_config,
                                       SocketContext_t *socket_context, const int8_t my_id,
                                       GameState_t *game_state, ClientState_t *client_state,
                                       SdlContext_t *sdl_context, Font_t *font,
                                       const SoundContext_t *sound_context, LinkWidget_t **links,
                                       const Path_t *path) {
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

  bool show_keys_overlay = false; /* F1 "Keys" reference panel */

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

  /* Card-room background: the seamless felt tile, replacing the flat green this
     screen used to clear to. No vignette here (lobby stays evenly lit). */
  SDL_Texture *felt_tex =
      load_coin_texture(sdl_context->renderer, path->data, "100x100-green-felt-seamless-tile.png");

  while (game_state->at_menu) {
    ERecvStatus_t recv_status = recv_game_state(socket_context, game_state, client_state, my_id);
    if (recv_status == RECV_ERROR) {
      result = GAME_SEL_ERROR;
      goto cleanup;
    }

    clear_screen(sdl_context->renderer);
    draw_felt_background(sdl_context->renderer, felt_tex);

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
      /* F1 toggles the read-only key list; while open it swallows other keys. */
      if (hotkey_overlay_handle_event(&e, &show_keys_overlay))
        continue;
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

    if (show_keys_overlay)
      hotkey_overlay_render(sdl_context->renderer, font, false);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

cleanup:
  SDL_DestroyTexture(felt_tex);
  ui_destroy_all(&registry);
  return result;
}
