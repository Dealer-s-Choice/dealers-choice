/*
 main.c
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

#include <canfigger.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h> // For setenv()
#include <string.h>

#include "client.h"
#include "config.h"
#include "dc_config.h"
#include "game.h"
#include "getlongopt.h"
#include "globals.h"
#include "graphics.h"
#include "links.h"
#include "main.h"
#include "server.h"
#include "util.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/image.h"
#include "widgets/input.h"
#include "widgets/text.h"
#include <sodium.h>

enum { RUN_CLIENT = 20, RUN_SETTINGS = 21 };

static int menu_display_connect(PlayerConfig_t *player_config, char *host_str, uint16_t *port,
                                SdlContext_t *sdl_context, Font_t *font, LinkWidget_t **links) {
  ButtonWidget_t *button_connect = button_widget_create(
      _("Connect"), (EColor_t){COLOR_BLACK, COLOR_YELLOW}, font->fonts[FONT_BOLD], (SDL_Keycode)0);
  ButtonWidget_t *button_settings = button_widget_create(
      _("Settings"), (EColor_t){COLOR_BLACK, COLOR_YELLOW}, font->fonts[FONT_BOLD], (SDL_Keycode)0);
  UIRegistry_t reg = {0};

  button_connect->base.rect.x = g_layout.menu.margin_x;
  button_connect->base.rect.y = g_layout.menu.connect_btn_y;
  button_settings->base.rect.x = g_layout.menu.margin_x + button_connect->base.rect.w + 20;
  button_settings->base.rect.y = g_layout.menu.connect_btn_y;

  int input_w;
  if (TTF_SizeUTF8(font->fonts[FONT_DEFAULT], "255.255.255.255", &input_w, NULL) != 0)
    input_w = 150;
  input_w += 20;

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
  port_input->base.rect.y = host_input->base.rect.y + host_input->base.rect.h + 20;
  ui_register(&reg, &port_input->base);

  ButtonWidget_t *button_save = button_widget_create(
      _("Save"), (EColor_t){COLOR_BLACK, COLOR_YELLOW}, font->fonts[FONT_BOLD], (SDL_Keycode)0);
  if (!button_save)
    goto err;
  ui_register(&reg, &button_save->base);

  ButtonWidget_t *button_defaults =
      button_widget_create(_("Load Defaults"), (EColor_t){COLOR_BLACK, COLOR_YELLOW},
                           font->fonts[FONT_BOLD], (SDL_Keycode)0);
  if (!button_defaults)
    goto err;
  ui_register(&reg, &button_defaults->base);
  button_save->base.rect.x = g_layout.menu.margin_x + input_w + 12;
  {
    int span_top = host_input->base.rect.y;
    int span_bot = port_input->base.rect.y + port_input->base.rect.h;
    button_save->base.rect.y = (span_top + span_bot) / 2 - button_save->base.rect.h / 2;
  }
  button_defaults->base.rect.x = button_save->base.rect.x + button_save->base.rect.w + 12;
  button_defaults->base.rect.y = button_save->base.rect.y;

  ButtonWidget_t *btn_quit_connect = button_widget_create("X", (EColor_t){COLOR_WHITE, COLOR_RED},
                                                          font->fonts[FONT_BOLD], (SDL_Keycode)0);
  if (btn_quit_connect) {
    btn_quit_connect->base.rect.x =
        g_viewport.x + g_viewport.w - btn_quit_connect->base.rect.w - MARGIN;
    btn_quit_connect->base.rect.y = g_layout.menu.quit_y;
    ui_register(&reg, &btn_quit_connect->base);
  }

  SDL_Rect input_nick_pos = {g_layout.menu.margin_x, port_input->base.rect.y + port_input->base.rect.h + 20, 0,
                             0};

  InputWidget_t *focused_inputs[2] = {host_input, port_input};
  int focused_slot = 0;

  SDL_StartTextInput();

  layout_links(links, LINK_DEFS_COUNT);

  TextWidget_t *tw_title = text_widget_create(DEALERSCHOICE_FORMAL_NAME, font->fonts[FONT_TITLE],
                                              get_color(COLOR_BLACK));
  if (tw_title)
    ui_widget_place(&tw_title->base, g_layout.menu.title_x, g_layout.menu.title_y);

  char version[64] = {0};
  snprintf(version, sizeof(version), "Version " DEALERSCHOICE_VERSION);
  TextWidget_t *tw_version =
      text_widget_create(version, font->fonts[FONT_VERSION], get_color(COLOR_WHITE));
  if (tw_version)
    ui_widget_place(&tw_version->base, g_layout.menu.title_x + 40, g_layout.menu.title_y + 80);

  TextWidget_t *tw_nick =
      text_widget_create(player_config->nick, font->fonts[FONT_DEFAULT], get_color(COLOR_BLACK));
  if (tw_nick)
    ui_widget_place(&tw_nick->base, input_nick_pos.x, input_nick_pos.y);

  bool run_client = false;
  bool run_settings = false;
  bool running = true;
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
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (btn_quit_connect && SDL_PointInRect(&mouse_pos, &btn_quit_connect->base.rect) &&
            confirm_quit(font->fonts[FONT_BOLD])) {
          SDL_Event quit = {.type = SDL_QUIT};
          SDL_PushEvent(&quit);
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &button_connect->base.rect)) {
          run_client = true;
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &button_settings->base.rect)) {
          run_settings = true;
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &button_save->base.rect)) {
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
          if (confirm_quit(font->fonts[FONT_BOLD])) {
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

    clear_screen(sdl_context->renderer);
    ui_widget_render(&button_connect->base);
    ui_widget_render(&button_settings->base);
    ui_render_all(&reg);

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
  ui_widget_destroy(&button_connect->base);
  ui_widget_destroy(&button_settings->base);
  ui_destroy_all(&reg);

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

static void menu_display_settings(PlayerConfig_t *player_config, SdlContext_t *sdl_context,
                                  Font_t *font, const Path_t *path) {
  /* Two-column layout for nick, language, volume, turn_notify (host/port on startup screen) */
  const int x_left         = g_layout.menu.margin_x;
  const int x_right        = g_layout.menu.settings_x_right;
  const int *row_y         = g_layout.menu.settings_row_y;
  const int input_y_offset = SETTINGS_INPUT_Y_OFFSET;
  const int input_w        = SETTINGS_INPUT_W;

  UIRegistry_t reg = {0};

  /* Back arrow image (top-left) */
  char *back_img_path = canfigger_path_join(path->data, "images/arrow_back.png");
  ImageWidget_t *back_img =
      back_img_path ? image_widget_create(back_img_path, back_btn_size, back_btn_size) : NULL;
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
  checkbox_size += 16;
  CheckboxWidget_t *turn_cb = checkbox_widget_create(init_checked, checkbox_size);
  if (turn_cb)
    ui_register(&reg, &turn_cb->base);

  /* Text inputs for displayed non-bool entries (skip host=1, port=2) */
  char init_str[MAX_PLAYER_CONFIG_ENTRIES][MAX_INPUT_LENGTH];
  InputWidget_t *inputs[MAX_PLAYER_CONFIG_ENTRIES];
  size_t n_text_inputs = 0;
  size_t display_pos = 0;
  for (size_t i = 0; i < player_config_entry_count; i++) {
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
    if (i != bool_idx && i != 1 && i != 2)
      text_input_indices[n_ti++] = i;
  int focused_slot = 0; /* index into text_input_indices */

  ButtonWidget_t *btn_save = button_widget_create(_("Save"), (EColor_t){COLOR_BLACK, COLOR_YELLOW},
                                                  font->fonts[FONT_BOLD], (SDL_Keycode)0);
  if (!btn_save) {
    ui_destroy_all(&reg);
    return;
  }
  btn_save->base.rect.x = x_left;
  btn_save->base.rect.y = g_layout.menu.settings_save_y;
  ui_register(&reg, &btn_save->base);

  ButtonWidget_t *btn_defaults =
      button_widget_create(_("Load Defaults"), (EColor_t){COLOR_BLACK, COLOR_YELLOW},
                           font->fonts[FONT_BOLD], (SDL_Keycode)0);
  if (!btn_defaults) {
    ui_destroy_all(&reg);
    return;
  }
  btn_defaults->base.rect.x = btn_save->base.rect.x + btn_save->base.rect.w + 20;
  btn_defaults->base.rect.y = btn_save->base.rect.y;
  ui_register(&reg, &btn_defaults->base);

  ButtonWidget_t *btn_quit_settings = button_widget_create("X", (EColor_t){COLOR_WHITE, COLOR_RED},
                                                           font->fonts[FONT_BOLD], (SDL_Keycode)0);
  if (btn_quit_settings) {
    btn_quit_settings->base.rect.x =
        g_viewport.x + g_viewport.w - btn_quit_settings->base.rect.w - MARGIN;
    btn_quit_settings->base.rect.y = g_layout.menu.quit_y;
    ui_register(&reg, &btn_quit_settings->base);
  }

  Uint32 anim_start = SDL_GetTicks();

  TextWidget_t *tw_settings_title =
      text_widget_create(_("Settings"), font->fonts[FONT_TITLE], get_color(COLOR_BLACK));
  if (tw_settings_title)
    ui_widget_place(&tw_settings_title->base, g_layout.menu.title_x, g_layout.menu.title_y);

  TextWidget_t *tw_labels[MAX_PLAYER_CONFIG_ENTRIES];
  for (size_t i = 0; i < player_config_entry_count; i++)
    tw_labels[i] = NULL;
  {
    size_t rpos = 0;
    for (size_t i = 0; i < player_config_entry_count; i++) {
      if (i == 1 || i == 2)
        continue;
      int col = (int)(rpos % 2);
      int row = (int)(rpos / 2);
      int lx = (col == 0) ? x_left : x_right;
      tw_labels[i] = text_widget_create(player_config_entries[i].key, font->fonts[FONT_DEFAULT],
                                        get_color(COLOR_BLACK));
      if (tw_labels[i])
        ui_widget_place(&tw_labels[i]->base, lx, row_y[row]);
      rpos++;
    }
  }

  SDL_StartTextInput();
  bool running = true;
  bool saved = false;

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_Point mouse_pos = {e.button.x, e.button.y};
      btn_save->base.hovered = SDL_PointInRect(&mouse_pos, &btn_save->base.rect);
      btn_defaults->base.hovered = SDL_PointInRect(&mouse_pos, &btn_defaults->base.rect);
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
            confirm_quit(font->fonts[FONT_BOLD])) {
          SDL_Event quit = {.type = SDL_QUIT};
          SDL_PushEvent(&quit);
          running = false;
        } else if (SDL_PointInRect(&mouse_pos, &btn_save->base.rect)) {
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
          if (confirm_quit(font->fonts[FONT_BOLD])) {
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
      int end_y = g_viewport.y + g_viewport.h - back_btn_size - 20;
      back_img->base.rect.y = start_y + (int)(t * (end_y - start_y));
    }

    ui_render_all(&reg);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);
  }

  SDL_StopTextInput();

  if (saved) {
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

static void print_version(void) {
  fputs(DEALERSCHOICE_FORMAL_NAME " v" DEALERSCHOICE_VERSION "\n", stdout);
  fputs(DEALERSCHOICE_URL "\n", stdout);
  putchar('\n');
}

static CliArgs_t parse_cli_args(int argc, char *argv[]) {
  enum {
    OPT_SERVER = 1,
    OPT_SERVER_LOG_GAME_RESULTS,
    OPT_SERVER_CONF,
    OPT_TEST,
    OPT_BIND,
    OPT_HOST,
    OPT_PORT,
    OPT_VERSION,
    OPT_VERBOSE,
    OPT_DISABLE_AUDIO,
    OPT_DISABLE_TIMEOUT,
    OPT_AUTODEAL,
    OPT_AUTO_CONNECT,
  };

  static const glopt_option_t options[] = {
      {"server", GLOPT_NO_ARG, OPT_SERVER, 0},
      {"server-log-game-results", GLOPT_REQUIRED_ARG, OPT_SERVER_LOG_GAME_RESULTS, 0},
      {"server-conf", GLOPT_REQUIRED_ARG, OPT_SERVER_CONF, 0},
      {"-test", GLOPT_NO_ARG, OPT_TEST, 0},
      {"bind-address", GLOPT_REQUIRED_ARG, OPT_BIND, 0},
      {"host", GLOPT_REQUIRED_ARG, OPT_HOST, 0},
      {"port", GLOPT_REQUIRED_ARG, OPT_PORT, 0},
      {"version", GLOPT_NO_ARG, OPT_VERSION, 0},
      {"verbose", GLOPT_NO_ARG, OPT_VERBOSE, 0},
      {"disable-audio", GLOPT_NO_ARG, OPT_DISABLE_AUDIO, 0},
      {"disable-timeout", GLOPT_NO_ARG, OPT_DISABLE_TIMEOUT, 0},
      {"autodeal", GLOPT_NO_ARG, OPT_AUTODEAL, 0},
      {"auto-connect", GLOPT_NO_ARG, OPT_AUTO_CONNECT, 0},
      {NULL, 0, 0, 0}};

  glopt_parser_t parser;
  glopt_init(&parser, options);
  CliArgs_t cli_args = {0};

  int opt;
  while ((opt = glopt_next(&parser, argc, argv)) != -1) {
    switch (opt) {
    case OPT_SERVER:
      cli_args.run_server_flag = true;
      break;
    case OPT_SERVER_LOG_GAME_RESULTS:
      cli_args.server_log_game_results_file = parser.optarg;
      cli_args.run_server_flag = true;
      break;
    case OPT_SERVER_CONF:
      cli_args.server_conf = parser.optarg;
      cli_args.run_server_flag = true;
      break;
    case OPT_TEST:
      cli_args.test_mode = true;
      break;
    case OPT_BIND:
      cli_args.bind_address = parser.optarg;
      break;
    case OPT_HOST:
      cli_args.host = parser.optarg;
      break;
    case OPT_PORT: {
      unsigned long port_val;
      parse_unsigned(parser.optarg, UINT16_MAX, &port_val);
      cli_args.port = (uint16_t)port_val;
      break;
    }
    case OPT_VERSION:
      print_version();
      exit(EXIT_SUCCESS);
      break;
    case OPT_VERBOSE:
      verbose = true;
      break;
    case OPT_DISABLE_AUDIO:
      cli_args.disable_audio = true;
      break;
    case OPT_DISABLE_TIMEOUT:
      cli_args.disable_timeout = true;
      break;
    case OPT_AUTODEAL:
      cli_args.autodeal = true;
      cli_args.disable_timeout = true;
      break;
    case OPT_AUTO_CONNECT:
      cli_args.auto_connect = true;
      break;
    case '?':
    default:
      print_version();
      fputs("Usage:\n"
            "  --verbose\n"
            "  --server-log-game-results [path/to/file]\n"
            "  --server-conf [Path to alternate server config file]\n"
            "  --bind-address [IP]        Address for the server to bind to (default: all "
            "interfaces)\n"
            "  --host [IP]\n"
            "  --port [port]\n"
            "  --disable-audio\n"
            "  --disable-timeout          Server will not disconnect players who exceed the action "
            "timeout threshold\n"
            "  --version\n",
            stderr);
      exit(EXIT_FAILURE);
    }
  }
  return cli_args;
}

static void init_sdl_window(SdlContext_t *c, const char *title) {
  SDL_Rect bounds;

  if (SDL_GetDisplayBounds(0, &bounds) != 0) {
    SDL_Log("SDL_GetDisplayBounds failed: %s", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  const float factor = 0.8f;
  const int w = (int)(bounds.w * factor);
  const int h = (int)(bounds.h * factor);

  c->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);

  if (!c->window) {
    SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  /* MUST be set BEFORE creating the renderer */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // nearest

  c->renderer =
      SDL_CreateRenderer(c->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  if (!c->renderer) {
    SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
    SDL_DestroyWindow(c->window);
    exit(EXIT_FAILURE);
  }

  if (SDL_RenderSetLogicalSize(c->renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT) != 0) {
    SDL_Log("SDL_RenderSetLogicalSize failed: %s", SDL_GetError());
  }

  SDL_RenderGetViewport(c->renderer, &g_viewport);
  SDL_Log("Viewport: x=%d y=%d w=%d h=%d", g_viewport.x, g_viewport.y, g_viewport.w, g_viewport.h);

  g_center.x = g_viewport.x + g_viewport.w / 2;
  g_center.y = g_viewport.y + g_viewport.h / 2;
}

int main(int argc, char *argv[]) {
#ifdef ENABLE_NLS
  static char *locale_dir;
  locale_dir = getenv("DEALERSCHOICE_LOCALEDIR");
  if (!locale_dir)
    locale_dir = DEALERSCHOICE_LOCALEDIR;

  setlocale(LC_ALL, "");
  bindtextdomain(DEALERSCHOICE_NAME, locale_dir);
  textdomain(DEALERSCHOICE_NAME);
#endif
  Path_t path = {0};
  get_data_dir(&path);

  const CliArgs_t cli_args = parse_cli_args(argc, argv);

  if (sodium_init() < 0) {
    fprintf(stderr, "libsodium init failed\n");
    exit(1);
  }

  pcg_srand_auto();

  if (cli_args.run_server_flag) {
    return run_server(&cli_args, &path);
  }

  if (SDL_Init(SDL_INIT_VIDEO) == -1) {
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }
  if (tcpme_init() != 0) {
    fprintf(stderr, "tcpme_init failed: %s\n", tcpme_get_error());
    SDL_Quit();
    return 1;
  }

  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return -1;
  }

  SdlContext_t sdl_context = {0};
  init_sdl_window(&sdl_context, DEALERSCHOICE_FORMAL_NAME);
  g_sdl_context = &sdl_context;

  Font_t font;

  const FontArgs_t font_args[] = {
      [FONT_CARD] = {.file = "LiberationSans-Bold.ttf", .ptsize = 38},
      [FONT_DEFAULT] = {.file = "LiberationSans-Regular.ttf", .ptsize = 32},
      [FONT_DEFAULT_BOLD] = {.file = "LiberationSans-Bold.ttf", .ptsize = 32},
      [FONT_BOLD] = {.file = "LiberationSans-Bold.ttf", .ptsize = 26},
      [FONT_LINK] = {.file = "LiberationSans-Regular.ttf", .ptsize = 22},
      [FONT_STATUS_MSG] = {.file = "LiberationSans-Bold.ttf", .ptsize = 24},
      [FONT_TITLE] = {.file = "LiberationSerif-BoldItalic.ttf", .ptsize = 72},
      [FONT_VERSION] = {.file = "LiberationSans-Regular.ttf", .ptsize = 22},
      [FONT_WILD_SELECT] = {.file = "LiberationSans-Bold.ttf", .ptsize = 24},
  };

  for (int i = 0; i < NUM_FONTS; ++i) {
    char font_path[4096] = {0};
    snprintf(font_path, sizeof(font_path), "%s/%s", path.data, font_args[i].file);
    font.fonts[i] = open_font(&(FontArgs_t){font_path, font_args[i].ptsize});
    if (!font.fonts[i])
      return -1;
  }

  layout_init(TTF_FontHeight(font.fonts[FONT_STATUS_MSG]));

  PlayerConfig_t player_config = get_player_config();
  if (!player_config.loaded) {
    fprintf(stderr, "Unable to load config\n");
    exit(EXIT_FAILURE);
  }

#ifdef ENABLE_NLS
  if (strlen(player_config.language) != 0) {
#ifdef _WIN32
    _putenv_s("LANGUAGE", player_config.language);
#else
    setenv("LANGUAGE", player_config.language, 1);
#endif
    setlocale(LC_ALL, "");
    bindtextdomain(DEALERSCHOICE_NAME, locale_dir);
    textdomain(DEALERSCHOICE_NAME);
  }
#endif

  LinkWidget_t *links[LINK_DEFS_COUNT];
  for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
    links[i] = link_widget_create(_(LINK_DEFS[i].text), LINK_DEFS[i].url, font.fonts[FONT_LINK]);
  layout_links(links, LINK_DEFS_COUNT);

  char host_str[MAX_INPUT_LENGTH] = {0};
  snprintf(host_str, sizeof(host_str), "%s", (cli_args.host) ? cli_args.host : player_config.host);

  uint16_t port = (cli_args.port != 0) ? cli_args.port : player_config.port;
  bool loop_to_connect = true;
  bool first_connect = true;
  while (loop_to_connect) {
    loop_to_connect = false;
    int connect_result;
    if (cli_args.auto_connect && !cli_args.run_server_flag && first_connect) {
      connect_result = RUN_CLIENT;
    } else {
      do {
        connect_result =
            menu_display_connect(&player_config, host_str, &port, &sdl_context, &font, links);
        if (connect_result == RUN_SETTINGS)
          menu_display_settings(&player_config, &sdl_context, &font, &path);
      } while (connect_result == RUN_SETTINGS);
    }

    if (connect_result == RUN_CLIENT) {
      bool went_back =
          get_socket_context_and_run_client(&player_config, &cli_args, host_str, port, &sdl_context,
                                            &font, &path, cli_args.test_mode, links, NULL);
      if (went_back) {
        loop_to_connect = true;
        first_connect = false;
      }
    }
  }

  for (size_t i = 0; i < LINK_DEFS_COUNT; i++)
    if (links[i])
      ui_widget_destroy(&links[i]->base);
  for (int i = 0; i < NUM_FONTS; ++i)
    TTF_CloseFont(font.fonts[i]);
  TTF_Quit();
  tcpme_quit();
  do_sdl_cleanup(&sdl_context);

  return 0;
}
