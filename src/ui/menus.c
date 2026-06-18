/*
 menus.c
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

/* The pre-game menu screens: connect (with LAN discovery list), hotkeys, and
 * settings. Split out of main.c; main() drives connect/settings via menus.h. */

#include <canfigger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "dc_config.h"
#include "game.h"
#include "globals_gui.h"
#include "graphics.h"
#include "hotkeys.h"
#include "lan_discovery.h"
#include "links.h"
#include "menus.h"
#include "util.h"
#include "widgets/button.h"
#include "widgets/card.h"
#include "widgets/card_text_atlas.h"
#include "widgets/checkbox.h"
#include "widgets/image.h"
#include "widgets/input.h"
#include "widgets/text.h"

enum { LAN_MAX_SHOWN = 6 };

static void lan_row_label(const LanGameInfo_t *g, char *buf, size_t n) {
  const char *who = (g->name[0] != '\0') ? g->name : g->ip;
  /* No password marker: the server password grants admin/bot privileges, it
   * does NOT gate joining, so showing it here would mislead a player picking a
   * server. */
  snprintf(buf, n, "%s  %u/%u%s", who, (unsigned)g->player_count, (unsigned)g->max_players,
           g->in_progress ? " [playing]" : "");
}

/* FNV-1a over the displayed fields, so the row buttons are rebuilt only when
 * the visible list actually changes. */
static uint64_t lan_list_sig(const LanGameInfo_t *list, int count) {
  uint64_t h = 1469598103934665603ULL;
  char buf[96];
  for (int i = 0; i < count; i++) {
    int m = snprintf(buf, sizeof(buf), "%s:%u:%u:%u:%d:%d|", list[i].ip, (unsigned)list[i].tcp_port,
                     (unsigned)list[i].player_count, (unsigned)list[i].max_players,
                     list[i].password_protected, list[i].in_progress);
    for (int j = 0; j < m && j < (int)sizeof(buf); j++) {
      h ^= (unsigned char)buf[j];
      h *= 1099511628211ULL;
    }
  }
  return h;
}

/* Insert g, or refresh an existing entry matched by ip+port. Returns the new
 * count (unchanged if the list is full). */
static int lan_upsert(LanGameInfo_t *list, uint32_t *seen, int count, int max,
                      const LanGameInfo_t *g, uint32_t now) {
  for (int i = 0; i < count; i++) {
    if (list[i].tcp_port == g->tcp_port && strcmp(list[i].ip, g->ip) == 0) {
      list[i] = *g;
      seen[i] = now;
      return count;
    }
  }
  if (count < max) {
    list[count] = *g;
    seen[count] = now;
    return count + 1;
  }
  return count;
}

/* Drop entries not seen within ttl ms; returns the compacted count. */
static int lan_expire(LanGameInfo_t *list, uint32_t *seen, int count, uint32_t now, uint32_t ttl) {
  int w = 0;
  for (int i = 0; i < count; i++) {
    if (now - seen[i] <= ttl) {
      if (w != i) {
        list[w] = list[i];
        seen[w] = seen[i];
      }
      w++;
    }
  }
  return w;
}

int menu_display_connect(PlayerConfig_t *player_config, char *host_str, uint16_t *port,
                                SdlContext_t *sdl_context, Font_t *font, LinkWidget_t **links) {
  ButtonWidget_t *button_connect =
      button_widget_create_styled(_("Connect"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  ButtonWidget_t *button_settings =
      button_widget_create_styled(_("Settings"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  UIRegistry_t reg = {0};

  button_connect->base.rect.x = g_layout.menu.margin_x;
  button_connect->base.rect.y = g_layout.menu.connect_btn_y;
  button_settings->base.rect.x =
      g_layout.menu.margin_x + button_connect->base.rect.w + g_layout_cfg.connect_settings_btn_gap;
  button_settings->base.rect.y = g_layout.menu.connect_btn_y;

  int input_w;
  if (TTF_SizeUTF8(font->fonts[FONT_DEFAULT], "255.255.255.255", &input_w, NULL) != 0)
    input_w = 150;
  input_w += g_layout_cfg.connect_input_w_pad;

  InputWidget_t *host_input =
      input_widget_create(host_str, font->fonts[FONT_DEFAULT], input_w, CFG_TYPE_STRING);
  if (!host_input)
    goto err;
  host_input->base.rect.x = g_layout.menu.margin_x;
  host_input->base.rect.y = g_layout.menu.connect_host_y;
  host_input->focused = true;
  ui_register(&reg, &host_input->base);

  char port_init[16] = {0};
  snprintf(port_init, sizeof(port_init), "%u", (unsigned)*port);
  InputWidget_t *port_input =
      input_widget_create(port_init, font->fonts[FONT_DEFAULT], input_w, CFG_TYPE_UINT16);
  if (!port_input)
    goto err;
  port_input->base.rect.x = g_layout.menu.margin_x;
  port_input->base.rect.y =
      host_input->base.rect.y + host_input->base.rect.h + g_layout_cfg.input_field_v_gap;
  ui_register(&reg, &port_input->base);

  ButtonWidget_t *button_save =
      button_widget_create_styled(_("Save"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (!button_save)
    goto err;
  button_save->interactive = false;
  ui_register(&reg, &button_save->base);

  ButtonWidget_t *button_defaults =
      button_widget_create_styled(_("Load Defaults"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (!button_defaults)
    goto err;
  ui_register(&reg, &button_defaults->base);
  button_save->base.rect.x = g_layout.menu.margin_x + input_w + g_layout_cfg.connect_save_btn_gap;
  {
    int span_top = host_input->base.rect.y;
    int span_bot = port_input->base.rect.y + port_input->base.rect.h;
    button_save->base.rect.y = (span_top + span_bot) / 2 - button_save->base.rect.h / 2;
  }
  button_defaults->base.rect.x =
      button_save->base.rect.x + button_save->base.rect.w + g_layout_cfg.connect_save_btn_gap;
  button_defaults->base.rect.y = button_save->base.rect.y;

  ButtonWidget_t *btn_quit_connect =
      button_widget_create_styled("X", &ROLE_DANGER, font->fonts, (SDL_Keycode)0);
  if (btn_quit_connect) {
    btn_quit_connect->base.rect.x =
        g_viewport.x + g_viewport.w - btn_quit_connect->base.rect.w - g_layout_cfg.margin;
    btn_quit_connect->base.rect.y = g_layout.menu.quit_y;
    ui_register(&reg, &btn_quit_connect->base);
  }

  SDL_Rect input_nick_pos = {
      g_layout.menu.margin_x,
      port_input->base.rect.y + port_input->base.rect.h + g_layout_cfg.input_field_v_gap, 0, 0};

  InputWidget_t *focused_inputs[2] = {host_input, port_input};
  int focused_slot = 0;

  SDL_StartTextInput();

  layout_links(links, LINK_DEFS_COUNT);

  TextWidget_t *tw_title =
      text_widget_create(DEALERSCHOICE_FORMAL_NAME, font->fonts[FONT_TITLE], DC_TEXT_ON_LIGHT);
  if (tw_title)
    ui_widget_place(&tw_title->base, g_layout.menu.title_x, g_layout.menu.title_y);

  char version[64] = {0};
  snprintf(version, sizeof(version), "Version " DEALERSCHOICE_VERSION);
  TextWidget_t *tw_version =
      text_widget_create(version, font->fonts[FONT_VERSION], DC_TEXT_ON_DARK);
  if (tw_version)
    ui_widget_place(&tw_version->base, g_layout.menu.title_x + g_layout_cfg.version_x_offset,
                    g_layout.menu.title_y + g_layout_cfg.version_y_offset);

  TextWidget_t *tw_nick =
      text_widget_create(player_config->nick, font->fonts[FONT_DEFAULT], DC_TEXT_ON_LIGHT);
  if (tw_nick)
    ui_widget_place(&tw_nick->base, input_nick_pos.x, input_nick_pos.y);

  /* Heading above the discovered-server rows; only drawn when at least one
   * server is found. */
  int lan_heading_y =
      input_nick_pos.y + TTF_FontHeight(font->fonts[FONT_DEFAULT]) + g_layout_cfg.input_field_v_gap;
  TextWidget_t *tw_lan_heading =
      text_widget_create(_("Servers on LAN"), font->fonts[FONT_DEFAULT_BOLD], DC_TEXT_ON_DARK);
  if (tw_lan_heading)
    ui_widget_place(&tw_lan_heading->base, g_layout.menu.margin_x, lan_heading_y);

  bool run_client = false;
  bool run_settings = false;
  bool running = true;

  /* LAN discovery: passively look for games on the network and show them as
   * clickable rows below the nick. Clicking a row connects to it immediately.
   * Optional — if the socket can't be opened, the screen just works manually. */
  tcpme_socket_t disc_sock = lan_discovery_open_client();
  tcpme_set_t *disc_set = NULL;
  if (tcpme_socket_valid(disc_sock)) {
    disc_set = tcpme_alloc_set(1);
    if (disc_set)
      tcpme_add_socket(disc_set, disc_sock);
    lan_discovery_query(disc_sock);
  }
  uint32_t last_query = SDL_GetTicks();
  LanGameInfo_t found[LAN_MAX_SHOWN];
  uint32_t found_seen[LAN_MAX_SHOWN];
  int found_count = 0;
  ButtonWidget_t *host_btn[LAN_MAX_SHOWN] = {0};
  int host_btn_count = 0;
  uint64_t shown_sig = 0;

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      button_connect->base.hovered = SDL_PointInRect(&mouse_pos, &button_connect->base.rect);
      button_settings->base.hovered = SDL_PointInRect(&mouse_pos, &button_settings->base.rect);
      button_save->base.hovered = SDL_PointInRect(&mouse_pos, &button_save->base.rect);
      button_defaults->base.hovered = SDL_PointInRect(&mouse_pos, &button_defaults->base.rect);
      if (btn_quit_connect)
        btn_quit_connect->base.hovered = SDL_PointInRect(&mouse_pos, &btn_quit_connect->base.rect);
      for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
        links[i]->base.hovered = SDL_PointInRect(&mouse_pos, &links[i]->base.rect);
      for (int i = 0; i < host_btn_count; i++)
        if (host_btn[i])
          host_btn[i]->base.hovered = SDL_PointInRect(&mouse_pos, &host_btn[i]->base.rect);
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (btn_quit_connect && SDL_PointInRect(&mouse_pos, &btn_quit_connect->base.rect) &&
            confirm_quit(font->fonts)) {
          SDL_Event quit = {.type = SDL_QUIT};
          SDL_PushEvent(&quit);
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &button_connect->base.rect)) {
          run_client = true;
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &button_settings->base.rect)) {
          run_settings = true;
          running = false;
        } else if (button_save->interactive &&
                   SDL_PointInRect(&mouse_pos, &button_save->base.rect)) {
          button_save->click.start_time = SDL_GetTicks();
          player_config_set_field(player_config, 1, input_widget_get_text(host_input));
          player_config_set_field(player_config, 2, input_widget_get_text(port_input));
          save_player_config(player_config);
        } else if (SDL_PointInRect(&mouse_pos, &button_defaults->base.rect)) {
          button_defaults->click.start_time = SDL_GetTicks();
          input_widget_set_text(host_input, player_config_entries[1].default_value);
          input_widget_set_text(port_input, player_config_entries[2].default_value);
        } else if (SDL_PointInRect(&mouse_pos, &host_input->base.rect)) {
          focused_inputs[focused_slot]->focused = false;
          focused_slot = 0;
          focused_inputs[focused_slot]->focused = true;
        } else if (SDL_PointInRect(&mouse_pos, &port_input->base.rect)) {
          focused_inputs[focused_slot]->focused = false;
          focused_slot = 1;
          focused_inputs[focused_slot]->focused = true;
        } else {
          for (int i = 0; i < host_btn_count; i++) {
            if (host_btn[i] && SDL_PointInRect(&mouse_pos, &host_btn[i]->base.rect)) {
              input_widget_set_text(host_input, found[i].ip);
              char pbuf[8];
              snprintf(pbuf, sizeof(pbuf), "%u", (unsigned)found[i].tcp_port);
              input_widget_set_text(port_input, pbuf);
              run_client = true;
              running = false;
              break;
            }
          }
        }
        for (size_t i = 0; i < LINK_DEFS_COUNT; i++) {
          if (links[i]->base.hovered && e.button.button == SDL_BUTTON_LEFT)
            if (SDL_OpenURL(links[i]->url) == -1)
              fputs(SDL_GetError(), stderr);
        }
      } else if (e.type == SDL_TEXTINPUT) {
        input_widget_append(focused_inputs[focused_slot], e.text.text);
      } else if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
        case SDLK_ESCAPE:
          if (confirm_quit(font->fonts)) {
            SDL_Event quit = {.type = SDL_QUIT};
            SDL_PushEvent(&quit);
            running = false;
          }
          break;

        case SDLK_RETURN:
          if (e.key.keysym.mod & KMOD_ALT)
            toggle_fullscreen(sdl_context);
          else {
            run_client = true;
            running = false;
          }
          break;

        case SDLK_F11:
          toggle_fullscreen(sdl_context);
          break;

        case SDLK_TAB: {
          focused_inputs[focused_slot]->focused = false;
          int dir = (e.key.keysym.mod & KMOD_SHIFT) ? -1 : 1;
          focused_slot = (focused_slot + dir + 2) % 2;
          focused_inputs[focused_slot]->focused = true;
          break;
        }

        case SDLK_BACKSPACE:
          input_widget_backspace(focused_inputs[focused_slot]);
          break;

        case SDLK_LEFT:
          input_widget_cursor_left(focused_inputs[focused_slot]);
          break;

        case SDLK_RIGHT:
          input_widget_cursor_right(focused_inputs[focused_slot]);
          break;

        case SDLK_HOME:
          input_widget_cursor_home(focused_inputs[focused_slot]);
          break;

        case SDLK_END:
          input_widget_cursor_end(focused_inputs[focused_slot]);
          break;

        case SDLK_v:
          if (e.key.keysym.mod & KMOD_CTRL) {
            char *clip = SDL_GetClipboardText();
            if (clip) {
              input_widget_append(focused_inputs[focused_slot], clip);
              SDL_free(clip);
            }
          }
          break;

        default:
          break;
        }
      }
    }

    /* --- LAN discovery: re-query, drain replies, expire, rebuild rows --- */
    if (disc_set) {
      uint32_t now = SDL_GetTicks();
      if (now - last_query >= 2000) {
        lan_discovery_query(disc_sock);
        last_query = now;
      }
      int drain = 0;
      while (drain++ < 32 && tcpme_check_sockets(disc_set, 0) > 0 &&
             tcpme_socket_ready(disc_set, disc_sock)) {
        LanGameInfo_t g;
        if (lan_discovery_read_response(disc_sock, &g))
          found_count = lan_upsert(found, found_seen, found_count, LAN_MAX_SHOWN, &g, now);
      }
      found_count = lan_expire(found, found_seen, found_count, now, 6000);

      uint64_t sig = lan_list_sig(found, found_count);
      if (sig != shown_sig) {
        for (int i = 0; i < host_btn_count; i++)
          if (host_btn[i])
            ui_widget_destroy(&host_btn[i]->base);
        host_btn_count = 0;
        int row_y = lan_heading_y +
                    (tw_lan_heading ? tw_lan_heading->base.rect.h + g_layout_cfg.input_field_v_gap : 0);
        for (int i = 0; i < found_count; i++) {
          char label[96];
          lan_row_label(&found[i], label, sizeof(label));
          host_btn[i] = button_widget_create_styled(label, &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
          if (!host_btn[i])
            continue;
          host_btn[i]->base.rect.x = g_layout.menu.margin_x;
          host_btn[i]->base.rect.y = row_y;
          row_y += host_btn[i]->base.rect.h + g_layout_cfg.input_field_v_gap;
          host_btn_count = i + 1;
        }
        shown_sig = sig;
      }
    }

    button_save->interactive =
        strcmp(input_widget_get_text(host_input), player_config->host) != 0 ||
        (uint16_t)strtoul(input_widget_get_text(port_input), NULL, 10) != player_config->port;

    clear_screen(sdl_context->renderer);
    ui_widget_render(&button_connect->base);
    ui_widget_render(&button_settings->base);
    ui_render_all(&reg);

    if (host_btn_count > 0 && tw_lan_heading)
      ui_widget_render(&tw_lan_heading->base);

    for (int i = 0; i < host_btn_count; i++)
      if (host_btn[i])
        ui_widget_render(&host_btn[i]->base);

    ui_widget_render(&tw_nick->base);
    ui_widget_render(&tw_title->base);
    ui_widget_render(&tw_version->base);

    for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
      ui_widget_render(&links[i]->base);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  SDL_StopTextInput();

  /* Copy final values back to the caller's variables */
  const char *final_host = input_widget_get_text(host_input);
  strncpy(host_str, final_host, MAX_INPUT_LENGTH - 1);
  host_str[MAX_INPUT_LENGTH - 1] = '\0';

  const char *final_port = input_widget_get_text(port_input);
  if (final_port && *final_port)
    *port = (uint16_t)strtoul(final_port, NULL, 10);

  if (tw_title)
    ui_widget_destroy(&tw_title->base);
  if (tw_version)
    ui_widget_destroy(&tw_version->base);
  if (tw_nick)
    ui_widget_destroy(&tw_nick->base);
  if (tw_lan_heading)
    ui_widget_destroy(&tw_lan_heading->base);
  ui_widget_destroy(&button_connect->base);
  ui_widget_destroy(&button_settings->base);
  ui_destroy_all(&reg);

  for (int i = 0; i < host_btn_count; i++)
    if (host_btn[i])
      ui_widget_destroy(&host_btn[i]->base);
  if (disc_set)
    tcpme_free_set(disc_set);
  if (tcpme_socket_valid(disc_sock))
    tcpme_close(disc_sock);

  if (run_client)
    return RUN_CLIENT;
  if (run_settings)
    return RUN_SETTINGS;
  return 0;

err:
  ui_widget_destroy(&button_connect->base);
  ui_widget_destroy(&button_settings->base);
  ui_destroy_all(&reg);
  return 0;
}

/* Keys the in-game loop already owns or that are kept for app-level use;
 * binding an action to one would shadow it.  Digits 1-8 are the bet-amount
 * buttons; the whole F1-F12 row is reserved (F11 fullscreen, F1 the planned
 * in-game hotkey overlay, the rest held for future shortcuts). */
static bool is_reserved_hotkey(SDL_Keycode k) {
  switch (k) {
  case SDLK_ESCAPE:
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
  case SDLK_TAB:
    return true;
  default:
    return (k >= SDLK_1 && k <= SDLK_8) || (k >= SDLK_F1 && k <= SDLK_F12);
  }
}

/* The configurable hotkeys are exactly the player-config entries whose key is
 * prefixed "hotkey_"; they are shown on the Hotkeys sub-screen, not the main
 * Settings grid. */
static bool is_hotkey_entry(size_t i) {
  return strncmp(player_config_entries[i].key, "hotkey_", 7) == 0;
}

/* Press-to-bind editor for the hotkey entries.  Each row shows an action and
 * its current key; clicking a row captures the next keypress.  Card-selection
 * and bet-amount digit keys are intentionally not editable. */
static void menu_display_hotkeys(PlayerConfig_t *player_config, SdlContext_t *sdl_context,
                                 Font_t *font, const Path_t *path) {
  const int label_x = g_layout.menu.margin_x;
  const int box_x = label_x + 300;
  const int box_w = 240;
  const int row_h = 70;
  int box_h;
  TTF_SizeUTF8(font->fonts[FONT_DEFAULT], "Ag", NULL, &box_h);
  box_h += g_layout_cfg.checkbox_pad;
  const int first_row_y = g_layout.menu.title_y + 130;

  /* Collect the hotkey entries (indices into player_config_entries). */
  size_t idx[MAX_PLAYER_CONFIG_ENTRIES];
  size_t n = 0;
  for (size_t i = 0; i < player_config_entry_count; i++)
    if (is_hotkey_entry(i))
      idx[n++] = i;

  UIRegistry_t reg = {0};

  char *back_img_path = canfigger_path_join(path->data, "images/arrow_back.png");
  ImageWidget_t *back_img = back_img_path
                                ? image_widget_create(back_img_path, g_layout_cfg.back_btn_size,
                                                      g_layout_cfg.back_btn_size)
                                : NULL;
  free(back_img_path);
  if (back_img) {
    back_img->base.rect.x = g_layout.menu.back_img_x;
    back_img->base.rect.y = g_layout.menu.back_img_y;
    ui_register(&reg, &back_img->base);
  }

  TextWidget_t *tw_title =
      text_widget_create(_("Hotkeys"), font->fonts[FONT_TITLE], DC_TEXT_ON_LIGHT);
  if (tw_title)
    ui_widget_place(&tw_title->base, g_layout.menu.title_x, g_layout.menu.title_y);

  TextWidget_t *tw_hint = text_widget_create(_("Click an action, then press a key"),
                                             font->fonts[FONT_DEFAULT], DC_TEXT_ON_LIGHT);
  if (tw_hint)
    ui_widget_place(&tw_hint->base, label_x, first_row_y - 50);

  /* Working copies of each key name, edited until Save. */
  char keyname[MAX_PLAYER_CONFIG_ENTRIES][SIZEOF_HOTKEY_NAME];
  TextWidget_t *tw_label[MAX_PLAYER_CONFIG_ENTRIES] = {0};
  TextWidget_t *tw_value[MAX_PLAYER_CONFIG_ENTRIES] = {0};
  SDL_Rect box[MAX_PLAYER_CONFIG_ENTRIES];

  for (size_t r = 0; r < n; r++) {
    const ConfigEntry *e = &player_config_entries[idx[r]];
    const char *field = (const char *)((const uint8_t *)player_config + e->offset);
    snprintf(keyname[r], sizeof(keyname[r]), "%s", field);

    char label[32];
    const char *suffix = strncmp(e->key, "hotkey_", 7) == 0 ? e->key + 7 : e->key;
    snprintf(label, sizeof(label), "%s", suffix);
    if (label[0])
      label[0] = (char)toupper((unsigned char)label[0]);

    int ry = first_row_y + (int)r * row_h;
    box[r] = (SDL_Rect){box_x, ry, box_w, box_h};

    tw_label[r] = text_widget_create(label, font->fonts[FONT_DEFAULT], DC_TEXT_ON_LIGHT);
    if (tw_label[r])
      ui_widget_place(&tw_label[r]->base, label_x, ry + 4);
    tw_value[r] = text_widget_create(keyname[r], font->fonts[FONT_DEFAULT], DC_TEXT_ON_DARK);
    if (tw_value[r])
      ui_widget_place(&tw_value[r]->base, box_x + 10, ry + 4);
  }

  ButtonWidget_t *btn_save =
      button_widget_create_styled(_("Save"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (btn_save) {
    btn_save->base.rect.x = label_x;
    btn_save->base.rect.y = first_row_y + (int)n * row_h + 20;
    btn_save->interactive = false;
    ui_register(&reg, &btn_save->base);
  }

  int capturing = -1; /* row index being bound, or -1 */
  bool running = true;
  Uint32 anim_start = SDL_GetTicks();

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      if (btn_save)
        btn_save->base.hovered = SDL_PointInRect(&mouse_pos, &btn_save->base.rect);
      if (back_img)
        back_img->base.hovered = SDL_PointInRect(&mouse_pos, &back_img->base.rect);

      if (e.type == SDL_QUIT) {
        SDL_PushEvent(&e);
        running = false;
      } else if (capturing >= 0 && e.type == SDL_KEYDOWN) {
        SDL_Keycode sym = e.key.keysym.sym;
        if (sym == SDLK_ESCAPE) {
          if (tw_value[capturing])
            text_widget_set_text(tw_value[capturing], keyname[capturing]);
          capturing = -1;
        } else if (is_reserved_hotkey(sym)) {
          /* Stay in capture mode so the user can pick a different key. */
          if (tw_hint)
            text_widget_set_text(tw_hint, _("That key is reserved - pick another"));
        } else {
          const char *kn = SDL_GetKeyName(sym);
          /* A key SDL can't name (some non-Latin layouts) is ignored so the
           * action keeps its previous, still-resolvable binding. */
          if (kn && *kn) {
            snprintf(keyname[capturing], sizeof(keyname[capturing]), "%s", kn);
            /* SDL names single keys uppercase ("C"); store lowercase to match
             * the lowercase defaults.  Multi-char names (Space) untouched. */
            if (keyname[capturing][1] == '\0')
              keyname[capturing][0] = (char)tolower((unsigned char)keyname[capturing][0]);
          }
          if (tw_value[capturing])
            text_widget_set_text(tw_value[capturing], keyname[capturing]);
          capturing = -1;
        }
      } else if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE)
          running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (btn_save && btn_save->interactive &&
            SDL_PointInRect(&mouse_pos, &btn_save->base.rect)) {
          btn_save->click.start_time = SDL_GetTicks();
          for (size_t r = 0; r < n; r++)
            player_config_set_field(player_config, idx[r], keyname[r]);
          save_player_config(player_config);
          init_hotkeys(player_config);
          running = false;
        } else if (back_img && SDL_PointInRect(&mouse_pos, &back_img->base.rect)) {
          running = false;
        } else {
          for (size_t r = 0; r < n; r++) {
            if (SDL_PointInRect(&mouse_pos, &box[r])) {
              capturing = (int)r;
              if (tw_value[r])
                text_widget_set_text(tw_value[r], _("press a key..."));
              if (tw_hint)
                text_widget_set_text(tw_hint, _("Click an action, then press a key"));
              break;
            }
          }
        }
      }
    }

    if (btn_save) {
      bool dirty = false;
      for (size_t r = 0; r < n && !dirty; r++) {
        const char *field =
            (const char *)((const uint8_t *)player_config + player_config_entries[idx[r]].offset);
        if (strcmp(keyname[r], field) != 0)
          dirty = true;
      }
      btn_save->interactive = dirty;
    }

    clear_screen(sdl_context->renderer);
    if (tw_title)
      ui_widget_render(&tw_title->base);
    if (tw_hint)
      ui_widget_render(&tw_hint->base);

    SDL_Point mp;
    SDL_GetMouseState(&mp.x, &mp.y);
    for (size_t r = 0; r < n; r++) {
      bool active = (capturing == (int)r);
      bool hover = SDL_PointInRect(&mp, &box[r]);
      SDL_SetRenderDrawColor(sdl_context->renderer, 25, 25, 25, 255);
      SDL_RenderFillRect(sdl_context->renderer, &box[r]);
      if (active)
        SDL_SetRenderDrawColor(sdl_context->renderer, 240, 204, 48, 255);
      else if (hover)
        SDL_SetRenderDrawColor(sdl_context->renderer, 200, 200, 200, 255);
      else
        SDL_SetRenderDrawColor(sdl_context->renderer, 110, 110, 110, 255);
      SDL_RenderDrawRect(sdl_context->renderer, &box[r]);
      if (tw_label[r])
        ui_widget_render(&tw_label[r]->base);
      if (tw_value[r])
        ui_widget_render(&tw_value[r]->base);
    }

    if (back_img) {
      float t = (SDL_GetTicks() - anim_start) / 1000.0f;
      if (t > 1.0f)
        t = 1.0f;
      int start_y = g_viewport.y + g_viewport.h * 2 / 3;
      int end_y = g_viewport.y + g_viewport.h - g_layout_cfg.back_btn_size - 20;
      back_img->base.rect.y = start_y + (int)(t * (end_y - start_y));
    }

    ui_render_all(&reg);
    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  for (size_t r = 0; r < n; r++) {
    if (tw_label[r])
      ui_widget_destroy(&tw_label[r]->base);
    if (tw_value[r])
      ui_widget_destroy(&tw_value[r]->base);
  }
  if (tw_title)
    ui_widget_destroy(&tw_title->base);
  if (tw_hint)
    ui_widget_destroy(&tw_hint->base);
  ui_destroy_all(&reg);
}

void menu_display_settings(PlayerConfig_t *player_config, SdlContext_t *sdl_context,
                                  Font_t *font, const Path_t *path) {
  /* Two-column layout for nick, language, volume, turn_notify (host/port on startup screen) */
  const int x_left = g_layout.menu.margin_x;
  const int x_right = g_layout.menu.settings_x_right;
  const int *row_y = g_layout.menu.settings_row_y;
  const int input_y_offset = g_layout_cfg.settings_input_y_offset;
  const int input_w = g_layout_cfg.settings_input_w;

  UIRegistry_t reg = {0};

  /* Back arrow image (top-left) */
  char *back_img_path = canfigger_path_join(path->data, "images/arrow_back.png");
  ImageWidget_t *back_img = back_img_path
                                ? image_widget_create(back_img_path, g_layout_cfg.back_btn_size,
                                                      g_layout_cfg.back_btn_size)
                                : NULL;
  free(back_img_path);
  if (back_img) {
    back_img->base.rect.x = g_layout.menu.back_img_x;
    back_img->base.rect.y = g_layout.menu.back_img_y;
    ui_register(&reg, &back_img->base);
  }

  /* Build input widgets; host(1) and port(2) are on the startup screen, not here.
   * BOOL entries get a CheckboxWidget_t instead of an InputWidget_t. */
  const size_t bool_idx = 5;
  const size_t password_idx = 7;

  /* Checkbox for the bool entry */
  const bool init_checked =
      *(const bool *)((const uint8_t *)player_config + player_config_entries[bool_idx].offset);
  int checkbox_size;
  TTF_SizeUTF8(font->fonts[FONT_DEFAULT], "Ag", NULL, &checkbox_size);
  checkbox_size += g_layout_cfg.checkbox_pad;
  CheckboxWidget_t *turn_cb = checkbox_widget_create(init_checked, checkbox_size);
  if (turn_cb)
    ui_register(&reg, &turn_cb->base);

  /* Text inputs for displayed non-bool entries (skip host=1, port=2) */
  char init_str[MAX_PLAYER_CONFIG_ENTRIES][MAX_INPUT_LENGTH];
  InputWidget_t *inputs[MAX_PLAYER_CONFIG_ENTRIES];
  size_t n_text_inputs = 0;
  size_t display_pos = 0;
  for (size_t i = 0; i < player_config_entry_count; i++) {
    if (is_hotkey_entry(i)) {
      inputs[i] = NULL;
      continue;
    }
    if (i == 1 || i == 2) {
      inputs[i] = NULL;
      continue;
    }
    if (i == bool_idx) {
      if (turn_cb) {
        int col = (int)(display_pos % 2);
        int row = (int)(display_pos / 2);
        turn_cb->base.rect.x = (col == 0) ? x_left : x_right;
        turn_cb->base.rect.y = row_y[row] + input_y_offset;
      }
      inputs[i] = NULL;
      display_pos++;
      continue;
    }
    const void *field = (const uint8_t *)player_config + player_config_entries[i].offset;
    switch (player_config_entries[i].type) {
    case CFG_TYPE_STRING:
      snprintf(init_str[i], sizeof(init_str[i]), "%s", (const char *)field);
      break;
    case CFG_TYPE_INT:
      snprintf(init_str[i], sizeof(init_str[i]), "%d", *(const int *)field);
      break;
    case CFG_TYPE_UINT8:
      snprintf(init_str[i], sizeof(init_str[i]), "%u", (unsigned)*(const uint8_t *)field);
      break;
    case CFG_TYPE_UINT16:
      snprintf(init_str[i], sizeof(init_str[i]), "%u", (unsigned)*(const uint16_t *)field);
      break;
    default:
      snprintf(init_str[i], sizeof(init_str[i]), "%s", player_config_entries[i].default_value);
      break;
    }
    inputs[i] = input_widget_create(init_str[i], font->fonts[FONT_DEFAULT], input_w,
                                    player_config_entries[i].type);
    if (!inputs[i]) {
      ui_destroy_all(&reg);
      return;
    }
    if (player_config_entries[i].type == CFG_TYPE_STRING && player_config_entries[i].size > 1)
      inputs[i]->max_len = player_config_entries[i].size - 1;
    ui_register(&reg, &inputs[i]->base);
    int col = (int)(display_pos % 2);
    int row = (int)(display_pos / 2);
    inputs[i]->base.rect.x = (col == 0) ? x_left : x_right;
    inputs[i]->base.rect.y = row_y[row] + input_y_offset;
    inputs[i]->focused = (n_text_inputs == 0);
    if (player_config_entries[i].type == CFG_TYPE_INT)
      inputs[i]->max_val = 10;
    if (i == password_idx)
      inputs[i]->masked = true;
    n_text_inputs++;
    display_pos++;
  }

  /* focused_slot indexes only the text input slots (skips host, port, bool_idx) */
  size_t text_input_indices[MAX_PLAYER_CONFIG_ENTRIES];
  size_t n_ti = 0;
  for (size_t i = 0; i < player_config_entry_count; i++)
    if (!is_hotkey_entry(i) && i != bool_idx && i != 1 && i != 2)
      text_input_indices[n_ti++] = i;
  int focused_slot = 0; /* index into text_input_indices */

  ButtonWidget_t *btn_save =
      button_widget_create_styled(_("Save"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (!btn_save) {
    ui_destroy_all(&reg);
    return;
  }
  btn_save->base.rect.x = x_left;
  btn_save->base.rect.y = g_layout.menu.settings_save_y;
  btn_save->interactive = false;
  ui_register(&reg, &btn_save->base);

  ButtonWidget_t *btn_defaults =
      button_widget_create_styled(_("Load Defaults"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (!btn_defaults) {
    ui_destroy_all(&reg);
    return;
  }
  btn_defaults->base.rect.x =
      btn_save->base.rect.x + btn_save->base.rect.w + g_layout_cfg.settings_save_btn_gap;
  btn_defaults->base.rect.y = btn_save->base.rect.y;
  ui_register(&reg, &btn_defaults->base);

  ButtonWidget_t *btn_hotkeys =
      button_widget_create_styled(_("Hotkeys"), &ROLE_PRIMARY, font->fonts, (SDL_Keycode)0);
  if (!btn_hotkeys) {
    ui_destroy_all(&reg);
    return;
  }
  btn_hotkeys->base.rect.x = g_layout.menu.settings_x_third;
  btn_hotkeys->base.rect.y = row_y[0];
  ui_register(&reg, &btn_hotkeys->base);

  ButtonWidget_t *btn_quit_settings =
      button_widget_create_styled("X", &ROLE_DANGER, font->fonts, (SDL_Keycode)0);
  if (btn_quit_settings) {
    btn_quit_settings->base.rect.x =
        g_viewport.x + g_viewport.w - btn_quit_settings->base.rect.w - g_layout_cfg.margin;
    btn_quit_settings->base.rect.y = g_layout.menu.quit_y;
    ui_register(&reg, &btn_quit_settings->base);
  }

  Uint32 anim_start = SDL_GetTicks();

  TextWidget_t *tw_settings_title =
      text_widget_create(_("Settings"), font->fonts[FONT_TITLE], DC_TEXT_ON_LIGHT);
  if (tw_settings_title)
    ui_widget_place(&tw_settings_title->base, g_layout.menu.title_x, g_layout.menu.title_y);

  TextWidget_t *tw_labels[MAX_PLAYER_CONFIG_ENTRIES];
  for (size_t i = 0; i < player_config_entry_count; i++)
    tw_labels[i] = NULL;
  {
    size_t rpos = 0;
    for (size_t i = 0; i < player_config_entry_count; i++) {
      if (is_hotkey_entry(i))
        continue;
      if (i == 1 || i == 2)
        continue;
      int col = (int)(rpos % 2);
      int row = (int)(rpos / 2);
      int lx = (col == 0) ? x_left : x_right;
      tw_labels[i] = text_widget_create(player_config_entries[i].key, font->fonts[FONT_DEFAULT],
                                        DC_TEXT_ON_LIGHT);
      if (tw_labels[i])
        ui_widget_place(&tw_labels[i]->base, lx, row_y[row]);
      rpos++;
    }
  }

  SDL_StartTextInput();
  bool running = true;
  bool saved = false;
  bool dirty = false;

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      btn_save->base.hovered = SDL_PointInRect(&mouse_pos, &btn_save->base.rect);
      btn_defaults->base.hovered = SDL_PointInRect(&mouse_pos, &btn_defaults->base.rect);
      btn_hotkeys->base.hovered = SDL_PointInRect(&mouse_pos, &btn_hotkeys->base.rect);
      if (back_img)
        back_img->base.hovered = SDL_PointInRect(&mouse_pos, &back_img->base.rect);
      if (btn_quit_settings)
        btn_quit_settings->base.hovered =
            SDL_PointInRect(&mouse_pos, &btn_quit_settings->base.rect);
      if (turn_cb)
        turn_cb->base.hovered = SDL_PointInRect(&mouse_pos, &turn_cb->base.rect);

      if (e.type == SDL_QUIT) {
        SDL_PushEvent(&e);
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (btn_quit_settings && SDL_PointInRect(&mouse_pos, &btn_quit_settings->base.rect) &&
            confirm_quit(font->fonts)) {
          SDL_Event quit = {.type = SDL_QUIT};
          SDL_PushEvent(&quit);
          running = false;
        } else if (btn_save->interactive && SDL_PointInRect(&mouse_pos, &btn_save->base.rect)) {
          btn_save->click.start_time = SDL_GetTicks();
          for (size_t i = 0; i < player_config_entry_count; i++) {
            if (i == bool_idx)
              player_config_set_field(player_config, i, turn_cb && turn_cb->checked ? "yes" : "no");
            else if (inputs[i])
              player_config_set_field(player_config, i, input_widget_get_text(inputs[i]));
          }
          save_player_config(player_config);
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &btn_defaults->base.rect)) {
          btn_defaults->click.start_time = SDL_GetTicks();
          for (size_t i = 0; i < player_config_entry_count; i++) {
            if (i == 0 || i == 1 || i == 2)
              continue;
            if (i == bool_idx) {
              if (turn_cb)
                turn_cb->checked =
                    strcmp(player_config_entries[bool_idx].default_value, "yes") == 0;
            } else if (inputs[i]) {
              input_widget_set_text(inputs[i], player_config_entries[i].default_value);
            }
          }
        } else if (SDL_PointInRect(&mouse_pos, &btn_hotkeys->base.rect)) {
          btn_hotkeys->click.start_time = SDL_GetTicks();
          menu_display_hotkeys(player_config, sdl_context, font, path);
        } else if (back_img && SDL_PointInRect(&mouse_pos, &back_img->base.rect)) {
          running = false;
        } else if (turn_cb && SDL_PointInRect(&mouse_pos, &turn_cb->base.rect)) {
          turn_cb->checked = !turn_cb->checked;
        } else {
          for (size_t s = 0; s < n_ti; s++) {
            size_t i = text_input_indices[s];
            if (SDL_PointInRect(&mouse_pos, &inputs[i]->base.rect)) {
              inputs[text_input_indices[focused_slot]]->focused = false;
              focused_slot = (int)s;
              inputs[text_input_indices[focused_slot]]->focused = true;
              break;
            }
          }
        }
      } else if (e.type == SDL_TEXTINPUT) {
        input_widget_append(inputs[text_input_indices[focused_slot]], e.text.text);
      } else if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
        case SDLK_RETURN:
          saved = true;
          running = false;
          break;
        case SDLK_ESCAPE:
          if (confirm_quit(font->fonts)) {
            SDL_Event quit = {.type = SDL_QUIT};
            SDL_PushEvent(&quit);
            running = false;
          }
          break;
        case SDLK_TAB: {
          if (n_ti == 0)
            break;
          inputs[text_input_indices[focused_slot]]->focused = false;
          int dir = (e.key.keysym.mod & KMOD_SHIFT) ? -1 : 1;
          focused_slot = (int)((focused_slot + dir + (int)n_ti) % (int)n_ti);
          inputs[text_input_indices[focused_slot]]->focused = true;
          break;
        }
        case SDLK_BACKSPACE:
          input_widget_backspace(inputs[text_input_indices[focused_slot]]);
          break;

        case SDLK_LEFT:
          input_widget_cursor_left(inputs[text_input_indices[focused_slot]]);
          break;

        case SDLK_RIGHT:
          input_widget_cursor_right(inputs[text_input_indices[focused_slot]]);
          break;

        case SDLK_HOME:
          input_widget_cursor_home(inputs[text_input_indices[focused_slot]]);
          break;

        case SDLK_END:
          input_widget_cursor_end(inputs[text_input_indices[focused_slot]]);
          break;

        case SDLK_v:
          if (e.key.keysym.mod & KMOD_CTRL) {
            char *clip = SDL_GetClipboardText();
            if (clip) {
              input_widget_append(inputs[text_input_indices[focused_slot]], clip);
              SDL_free(clip);
            }
          }
          break;

        case SDLK_F11:
          toggle_fullscreen(sdl_context);
          break;
        default:
          break;
        }
      }
    }

    dirty = turn_cb && turn_cb->checked != init_checked;
    for (size_t i = 0; i < player_config_entry_count && !dirty; i++)
      if (inputs[i] && strcmp(input_widget_get_text(inputs[i]), init_str[i]) != 0)
        dirty = true;
    btn_save->interactive = dirty;

    clear_screen(sdl_context->renderer);

    ui_widget_render(&tw_settings_title->base);

    for (size_t i = 0; i < player_config_entry_count; i++)
      if (tw_labels[i])
        ui_widget_render(&tw_labels[i]->base);

    if (back_img) {
      float t = (SDL_GetTicks() - anim_start) / 1000.0f;
      if (t > 1.0f)
        t = 1.0f;
      int start_y = g_viewport.y + g_viewport.h * 2 / 3;
      int end_y = g_viewport.y + g_viewport.h - g_layout_cfg.back_btn_size - 20;
      back_img->base.rect.y = start_y + (int)(t * (end_y - start_y));
    }

    ui_render_all(&reg);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  SDL_StopTextInput();

  if (saved && dirty) {
    for (size_t i = 0; i < player_config_entry_count; i++) {
      if (i == bool_idx)
        player_config_set_field(player_config, i, turn_cb && turn_cb->checked ? "yes" : "no");
      else if (inputs[i])
        player_config_set_field(player_config, i, input_widget_get_text(inputs[i]));
    }
    save_player_config(player_config);
  }

  if (tw_settings_title)
    ui_widget_destroy(&tw_settings_title->base);
  for (size_t i = 0; i < player_config_entry_count; i++)
    if (tw_labels[i])
      ui_widget_destroy(&tw_labels[i]->base);
  ui_destroy_all(&reg);
}
