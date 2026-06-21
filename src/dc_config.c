/*
 dc_config.c
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
#include <stdlib.h>
#include <string.h>

#include "dc_config.h"
#include "dc_windows.h"
#include "layout_config.h"
#include "net/lan_discovery.h"
#include "net/registry.h"
#include "util.h"

/* Atomically replace dst with src. POSIX rename() does this directly; on
 * Windows rename() fails if dst exists, so use MoveFileExA with replace. */
static int replace_file(const char *src, const char *dst) {
#ifdef _WIN32
  if (!MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING))
    return -1;
  return 0;
#else
  return rename(src, dst);
#endif
}

const ConfigEntry player_config_entries[] = {
    {"nick", CFG_TYPE_STRING, "New Player", offsetof(PlayerConfig_t, nick),
     sizeof(((PlayerConfig_t *)0)->nick)},
    {"host", CFG_TYPE_STRING, "127.0.0.1", offsetof(PlayerConfig_t, host),
     sizeof(((PlayerConfig_t *)0)->host)},
    {"port", CFG_TYPE_UINT16, DEFAULT_PORT, offsetof(PlayerConfig_t, port), sizeof(uint16_t)},
    {"language", CFG_TYPE_STRING, "", offsetof(PlayerConfig_t, language),
     sizeof(((PlayerConfig_t *)0)->language)},
    {"sound.volume", CFG_TYPE_INT, "5", offsetof(PlayerConfig_t, volume), sizeof(int)},
    {"sound.notify.turn", CFG_TYPE_BOOL, "yes", offsetof(PlayerConfig_t, turn_notify),
     sizeof(bool)},
    {"connect.attempts", CFG_TYPE_UINT8, "6", offsetof(PlayerConfig_t, connect_attempts),
     sizeof(uint8_t)},
    {"password", CFG_TYPE_STRING, "", offsetof(PlayerConfig_t, password),
     sizeof(((PlayerConfig_t *)0)->password)},
    {"registry_browser", CFG_TYPE_BOOL, "yes", offsetof(PlayerConfig_t, registry_browser),
     sizeof(bool)},
#define HK(name, def)                                                                              \
  {"hotkey_" #name, CFG_TYPE_STRING, def, offsetof(PlayerConfig_t, hotkey_##name),                 \
   SIZEOF_HOTKEY_NAME}
    HK(check, "c"),
    HK(bet, "b"),
    HK(fold, "f"),
    HK(call, "l"),
    HK(raise, "r"),
    HK(complete, "o"),
    HK(discard, "d"),
    HK(hand_rank, "h"),
#undef HK
    {0}};

const ConfigEntry server_config_entries[] = {
    {"bind_address", CFG_TYPE_STRING, "127.0.0.1", offsetof(ServerConfig_t, bind_address),
     sizeof(((ServerConfig_t *)0)->bind_address)},
    {"port", CFG_TYPE_UINT16, DEFAULT_PORT, offsetof(ServerConfig_t, port), sizeof(uint16_t)},
    {"server_name", CFG_TYPE_STRING, "Dealer's Choice server",
     offsetof(ServerConfig_t, server_name), sizeof(((ServerConfig_t *)0)->server_name)},
    {"end_of_game_timeout", CFG_TYPE_UINT32, "15", offsetof(ServerConfig_t, end_of_game_timeout_ms),
     sizeof(uint32_t)},
    {"action_timeout", CFG_TYPE_UINT32, "30", offsetof(ServerConfig_t, action_timeout_ms),
     sizeof(uint32_t)},
    {"dealer_timeout", CFG_TYPE_UINT32, "60", offsetof(ServerConfig_t, dealer_timeout_ms),
     sizeof(uint32_t)},
    {"ante", CFG_TYPE_UINT32, "50", offsetof(ServerConfig_t, ante), sizeof(uint32_t)},
    {"bringin_amount", CFG_TYPE_UINT32, "50", offsetof(ServerConfig_t, bringin_amount),
     sizeof(uint32_t)},
    {"starting_coins", CFG_TYPE_INT, "20000", offsetof(ServerConfig_t, starting_coins),
     sizeof(int32_t)},
    {"max_raises", CFG_TYPE_UINT32, "3", offsetof(ServerConfig_t, max_raises), sizeof(uint32_t)},
    {"action_timeout_max", CFG_TYPE_UINT8, "3", offsetof(ServerConfig_t, action_timeout_max),
     sizeof(uint8_t)},
    {"password", CFG_TYPE_STRING, "", offsetof(ServerConfig_t, password),
     sizeof(((ServerConfig_t *)0)->password)},
    {"max_connections_per_minute", CFG_TYPE_UINT32, "10",
     offsetof(ServerConfig_t, max_connections_per_minute), sizeof(uint32_t)},
    {"max_connections_per_ip", CFG_TYPE_UINT32, "0",
     offsetof(ServerConfig_t, max_connections_per_ip), sizeof(uint32_t)},
    {0}};

const size_t player_config_entry_count = ARRAY_SIZE(player_config_entries) - 1;
const size_t server_config_entry_count = ARRAY_SIZE(server_config_entries) - 1;

_Static_assert(ARRAY_SIZE(player_config_entries) - 1 <= MAX_PLAYER_CONFIG_ENTRIES,
               "Too many player config entries; increase MAX_PLAYER_CONFIG_ENTRIES");
_Static_assert(ARRAY_SIZE(server_config_entries) - 1 <= MAX_SERVER_CONFIG_ENTRIES,
               "Too many server config entries; increase MAX_SERVER_CONFIG_ENTRIES");

#define LC(key, default_val, field)                                                                \
  {key, CFG_TYPE_INT, default_val, offsetof(LayoutConfig_t, field), sizeof(int)}

const ConfigEntry layout_config_entries[] = {
    LC("margin", "20", margin),
    LC("button_x_spacing", "10", button_x_spacing),
    LC("back_btn_size", "96", back_btn_size),
    LC("circle_timer_r", "50", circle_timer_r),
    LC("msg_panel_x_offset", "30", msg_panel_x_offset),
    LC("msg_panel_w", "420", msg_panel_w),
    LC("msg_panel_pad_x", "8", msg_panel_pad_x),
    LC("msg_panel_pad_y", "6", msg_panel_pad_y),
    LC("settings_input_w", "350", settings_input_w),
    LC("settings_input_y_offset", "40", settings_input_y_offset),
    LC("card_w", "80", card_w),
    LC("card_h", "50", card_h),
    LC("card_padding", "10", card_padding),
    LC("link_pad_x", "10", link_pad_x),
    LC("link_pad_y", "2", link_pad_y),
    LC("action_btn_x_gap", "50", action_btn_x_gap),
    LC("menu_title_y", "60", menu_title_y),
    LC("menu_margin_x_offset", "100", menu_margin_x_offset),
    LC("menu_connect_btn_y_offset", "160", menu_connect_btn_y_offset),
    LC("menu_connect_host_y_offset", "220", menu_connect_host_y_offset),
    LC("menu_settings_x_right_offset", "700", menu_settings_x_right_offset),
    LC("menu_settings_x_third_offset", "1300", menu_settings_x_third_offset),
    LC("menu_settings_row_y_0", "160", menu_settings_row_y_0),
    LC("menu_settings_row_y_1", "360", menu_settings_row_y_1),
    LC("menu_settings_row_y_2", "560", menu_settings_row_y_2),
    LC("menu_settings_save_y_offset", "750", menu_settings_save_y_offset),
    LC("menu_links_center_x_offset", "200", menu_links_center_x_offset),
    LC("lobby_waiting_from_bottom", "200", lobby_waiting_from_bottom),
    LC("lobby_kick_x_divisor", "10", lobby_kick_x_divisor),
    LC("lobby_kick_y_pct", "82", lobby_kick_y_pct),
    LC("kick_ban_btn_gap", "16", kick_ban_btn_gap),
    LC("game_kick_y_gap", "20", game_kick_y_gap),
    LC("pot_boundary", "450", pot_boundary),
    LC("board_y_offset", "40", board_y_offset),
    LC("timer_border", "10", timer_border),
    LC("indicator_pad", "14", indicator_pad),
    LC("indicator_min_r", "24", indicator_min_r),
    LC("nameplate_pad", "20", nameplate_pad),
    LC("open_card_pad", "20", open_card_pad),
    LC("nameplate_radius", "20", nameplate_radius),
    LC("confirm_quit_pad", "40", confirm_quit_pad),
    LC("confirm_quit_btn_gap", "20", confirm_quit_btn_gap),
    LC("connect_settings_btn_gap", "20", connect_settings_btn_gap),
    LC("input_field_v_gap", "20", input_field_v_gap),
    LC("connect_input_w_pad", "20", connect_input_w_pad),
    LC("connect_save_btn_gap", "12", connect_save_btn_gap),
    LC("settings_save_btn_gap", "20", settings_save_btn_gap),
    LC("version_x_offset", "40", version_x_offset),
    LC("version_y_offset", "80", version_y_offset),
    LC("checkbox_pad", "16", checkbox_pad),
    LC("input_text_pad_x", "8", input_text_pad_x),
    LC("input_h_pad", "16", input_h_pad),
    LC("timer_status_gap", "50", timer_status_gap),
    LC("community_top_offset", "60", community_top_offset),
    LC("discard_overlay_alpha", "180", discard_overlay_alpha),
    LC("btn_hand_gap", "20", btn_hand_gap),
    LC("dash_pad", "12", dash_pad),
    LC("dash_divider", "20", dash_divider),
    LC("dash_row_div", "8", dash_row_div),
    LC("dash_btn_div", "4", dash_btn_div),
    LC("amount_btn_min_w", "64", amount_btn_min_w),
    LC("slider_w", "320", slider_w),
    LC("indicator_cell_pad", "20", indicator_cell_pad),
    LC("dash_x_offset", "220", dash_x_offset),
    LC("act_btn_gap", "20", act_btn_gap),
    {0}};

#undef LC

const size_t layout_config_entry_count = ARRAY_SIZE(layout_config_entries) - 1;

_Static_assert(ARRAY_SIZE(layout_config_entries) - 1 <= MAX_LAYOUT_CONFIG_ENTRIES,
               "Too many layout config entries; increase MAX_LAYOUT_CONFIG_ENTRIES");

#define CFG_SET_SIGNED(TYPE, MIN, MAX)                                                             \
  do {                                                                                             \
    long v;                                                                                        \
    parse_signed(val, (MIN), (MAX), &v);                                                           \
    *(TYPE *)field = (TYPE)v;                                                                      \
  } while (0)

#define CFG_SET_UNSIGNED(TYPE, MAX)                                                                \
  do {                                                                                             \
    unsigned long v;                                                                               \
    parse_unsigned(val, (MAX), &v);                                                                \
    *(TYPE *)field = (TYPE)v;                                                                      \
  } while (0)

static void config_set_from_string_real(void *cfg, const ConfigEntry *entry, const char *val) {
  void *field = (uint8_t *)cfg + entry->offset;

  switch (entry->type) {
  case CFG_TYPE_STRING:
    snprintf((char *)field, entry->size, "%s", val);
    break;
  case CFG_TYPE_INT:
    CFG_SET_SIGNED(int, INT_MIN, INT_MAX);
    break;

  case CFG_TYPE_UINT8:
    CFG_SET_UNSIGNED(uint8_t, UINT8_MAX);
    break;

  case CFG_TYPE_UINT16:
    CFG_SET_UNSIGNED(uint16_t, UINT16_MAX);
    break;

  case CFG_TYPE_UINT32:
    CFG_SET_UNSIGNED(uint32_t, UINT32_MAX);
    break;

  case CFG_TYPE_BOOL:
    if (strcasecmp(val, "yes") == 0 || strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0) {
      *(bool *)field = true;
    } else {
      *(bool *)field = false;
    }
    break;
  }
}

static void config_field_to_string(const void *cfg, const ConfigEntry *entry, char *out,
                                   size_t out_size) {
  const void *field = (const uint8_t *)cfg + entry->offset;
  switch (entry->type) {
  case CFG_TYPE_STRING:
    snprintf(out, out_size, "%s", (const char *)field);
    break;
  case CFG_TYPE_INT:
    snprintf(out, out_size, "%d", *(const int *)field);
    break;
  case CFG_TYPE_UINT8:
    snprintf(out, out_size, "%u", (unsigned)*(const uint8_t *)field);
    break;
  case CFG_TYPE_UINT16:
    snprintf(out, out_size, "%u", (unsigned)*(const uint16_t *)field);
    break;
  case CFG_TYPE_UINT32:
    snprintf(out, out_size, "%u", (unsigned)*(const uint32_t *)field);
    break;
  case CFG_TYPE_BOOL:
    snprintf(out, out_size, "%s", *(const bool *)field ? "yes" : "no");
    break;
  }
}

void save_player_config(const PlayerConfig_t *config) {
  char *cfgdir = canfigger_config_dir(DEALERSCHOICE_NAME);
  if (!cfgdir) {
    dc_log(DC_LOG_ERROR, "Unable to determine config directory.");
    return;
  }

  char *cfg_pathname = canfigger_path_join(cfgdir, "player.conf");
  free(cfgdir);
  if (!cfg_pathname) {
    dc_log(DC_LOG_ERROR, "canfigger_path_join failed");
    return;
  }

  size_t tmp_len = strlen(cfg_pathname) + sizeof(".tmp");
  char *tmp_pathname = malloc(tmp_len);
  if (!tmp_pathname) {
    dc_log(DC_LOG_ERROR, "malloc: %s", strerror(errno));
    free(cfg_pathname);
    return;
  }
  snprintf(tmp_pathname, tmp_len, "%s.tmp", cfg_pathname);

  FILE *fp = fopen(tmp_pathname, "w");
  if (!fp) {
    dc_log(DC_LOG_ERROR, "fopen: %s", strerror(errno));
    free(tmp_pathname);
    free(cfg_pathname);
    return;
  }

  char val_str[MAX_INPUT_LENGTH];
  for (size_t i = 0; i < player_config_entry_count; i++) {
    config_field_to_string(config, &player_config_entries[i], val_str, sizeof(val_str));
    fprintf(fp, "%s = %s\n", player_config_entries[i].key, val_str);
  }

  bool write_ok = (ferror(fp) == 0);
  if (fclose(fp) != 0)
    write_ok = false;

  if (!write_ok) {
    dc_log(DC_LOG_ERROR, "writing player.conf: %s", strerror(errno));
    remove(tmp_pathname);
  } else if (replace_file(tmp_pathname, cfg_pathname) != 0) {
    dc_log(DC_LOG_ERROR, "rename player.conf: %s", strerror(errno));
    remove(tmp_pathname);
  }

  free(tmp_pathname);
  free(cfg_pathname);
}

void player_config_set_field(PlayerConfig_t *cfg, size_t entry_idx, const char *val) {
  if (entry_idx >= player_config_entry_count)
    return;
  config_set_from_string_real(cfg, &player_config_entries[entry_idx], val);
}

/* Read the shared registry list from <data_dir>/common.conf (read by both the
 * server, to publish, and the client, to browse). dnsmasq-style: one registry
 * per line, the host is the value and an optional port is the first attribute:
 *   registry = registry.example.org
 *   registry = 203.0.113.5, 22071
 * The port defaults to REGISTRY_DEFAULT_PORT. Missing file/key is fine. */
void get_common_registries(const char *data_dir, char host[][REGISTRY_HOST_LEN], uint16_t *port,
                           uint8_t *count, uint16_t *lan_discovery_port) {
  *count = 0;
  *lan_discovery_port = LAN_DISCOVERY_PORT; /* default; common.conf may override */
  char *cfg_pathname = canfigger_path_join(data_dir, "common.conf");
  if (!cfg_pathname)
    return;
  /* common.conf ships with every install, so a missing/unreadable one means a
   * broken or incomplete install — fail loudly. An existing but empty/all-
   * commented file is a valid LAN-only / no-registry config (it just yields zero
   * registries below); only the open failure is fatal. */
  FILE *probe = fopen(cfg_pathname, "r");
  if (!probe) {
    dc_log(DC_LOG_ERROR, "cannot open %s (%s); the install looks incomplete (common.conf is required)",
           cfg_pathname, strerror(errno));
    free(cfg_pathname);
    exit(EXIT_FAILURE);
  }
  fclose(probe);
  struct Canfigger *cfg_node = canfigger_parse_file(cfg_pathname, ',');
  free(cfg_pathname);
  while (cfg_node) {
    if (strcasecmp(cfg_node->key, "registry") == 0 && cfg_node->value && *count < MAX_REGISTRIES) {
      snprintf(host[*count], REGISTRY_HOST_LEN, "%s", cfg_node->value);
      unsigned long p = strtoul(REGISTRY_DEFAULT_PORT, NULL, 10);
      char *attr = NULL;
      canfigger_free_current_attr_str_advance(cfg_node->attributes, &attr);
      bool first = true;
      while (attr) {
        if (first) {
          parse_unsigned(attr, UINT16_MAX, &p); /* exits on a malformed port */
          first = false;
        }
        canfigger_free_current_attr_str_advance(cfg_node->attributes, &attr);
      }
      port[*count] = (uint16_t)p;
      (*count)++;
    } else if (strcasecmp(cfg_node->key, "lan_discovery_port") == 0 && cfg_node->value) {
      unsigned long p;
      parse_unsigned(cfg_node->value, UINT16_MAX, &p); /* exits on a malformed port */
      *lan_discovery_port = (uint16_t)p;
    }
    canfigger_free_current_key_node_advance(&cfg_node);
  }
}

ServerConfig_t get_server_config(Path_t *path, const CliArgs_t *cli_args) {
  ServerConfig_t config = {0};

  if (!cli_args->server_conf) {
    path->server_conf_name = canfigger_path_join(path->data, "server.conf");
    if (!path->server_conf_name) {
      dc_log(DC_LOG_ERROR, "canfigger_path_join failed");
      return config;
    }
  } else {
    path->server_conf_name = expand_tilde(cli_args->server_conf);
  }

  printf("Reading server config: %s\n", path->server_conf_name);
  struct Canfigger *cfg_node = canfigger_parse_file(path->server_conf_name, ',');
  free(path->server_conf_name);
  if (!cfg_node)
    exit(EXIT_FAILURE);

  // Track which keys were found
  bool found_keys[MAX_SERVER_CONFIG_ENTRIES];
  memset(found_keys, 0, sizeof(found_keys));

  bool found_bet_amounts = false;

  while (cfg_node) {
    if (strcasecmp(cfg_node->key, "bet_amounts") == 0) {
      found_bet_amounts = true;
      char *attr = NULL;
      canfigger_free_current_attr_str_advance(cfg_node->attributes, &attr);
      while (attr && config.bet_amount_count < MAX_BET_AMOUNTS) {
        unsigned long v;
        parse_unsigned(attr, UINT32_MAX, &v);
        config.bet_amounts[config.bet_amount_count++] = (uint32_t)v;
        canfigger_free_current_attr_str_advance(cfg_node->attributes, &attr);
      }
    } else {
      for (size_t i = 0; i < server_config_entry_count; i++) {
        if (strcasecmp(cfg_node->key, server_config_entries[i].key) == 0) {
          config_set_from_string_real(&config, &server_config_entries[i],
                                      cfg_node->value ? cfg_node->value
                                                      : server_config_entries[i].default_value);
          found_keys[i] = true;
          break;
        }
      }
    }

    canfigger_free_current_key_node_advance(&cfg_node);
  }

  if (!found_bet_amounts) {
    config.bet_amounts[0] = 100;
    config.bet_amounts[1] = 250;
    config.bet_amounts[2] = 500;
    config.bet_amount_count = 3;
  }

  for (size_t i = 0; i < server_config_entry_count; i++)
    if (!found_keys[i])
      config_set_from_string_real(&config, &server_config_entries[i],
                                  server_config_entries[i].default_value);

  config.end_of_game_timeout_ms *= 1000;
  config.action_timeout_ms *= 1000;
  config.dealer_timeout_ms *= 1000;

  // DC_PASSWORD env var takes precedence over server.conf
  const char *env_pw = getenv("DC_PASSWORD");
  if (env_pw)
    snprintf(config.password, sizeof(config.password), "%s", env_pw);

  /* Registries to publish to (and the LAN-discovery port) come from the shared
   * common.conf, not server.conf. A --discovery-port CLI flag overrides it. */
  get_common_registries(path->data, config.registry_host, config.registry_port,
                        &config.registry_count, &config.lan_discovery_port);
  if (cli_args->discovery_port != 0)
    config.lan_discovery_port = cli_args->discovery_port;

  return config;
}

PlayerConfig_t get_player_config(void) {
  PlayerConfig_t config = {0};
  config.loaded = false;

  char *cfgdir = canfigger_config_dir(DEALERSCHOICE_NAME);
  if (!cfgdir) {
    dc_log(DC_LOG_ERROR, "Unable to determine config directory.");
    return config;
  }

  EPathState state = check_pathname_state(cfgdir);
  if (state == PATH_NOT_FOUND) {
    if (make_directory_recursive(cfgdir) != 0) {
      dc_log(DC_LOG_ERROR, "Failed to create config dir: %s", cfgdir);
      free(cfgdir);
      return config;
    }
  } else if (state == PATH_ERROR) {
    dc_log(DC_LOG_ERROR, "Error checking config dir: %s", cfgdir);
    free(cfgdir);
    return config;
  }

  char *cfg_pathname = canfigger_path_join(cfgdir, "player.conf");
  free(cfgdir);
  if (!cfg_pathname) {
    dc_log(DC_LOG_ERROR, "canfigger_path_join failed");
    return config;
  }

  printf("Reading config: %s\n", cfg_pathname);
  struct Canfigger *cfg_node = canfigger_parse_file(cfg_pathname, ',');
  if (!cfg_node) {
    if (check_pathname_state(cfg_pathname) == PATH_NOT_FOUND) {
      printf("Creating %s\n", cfg_pathname);
      FILE *fp = fopen(cfg_pathname, "w");
      if (fp) {
        for (size_t i = 0; i < player_config_entry_count; i++) {
          fprintf(fp, "%s = %s\n", player_config_entries[i].key,
                  player_config_entries[i].default_value);
          config_set_from_string_real(&config, &player_config_entries[i],
                                      player_config_entries[i].default_value);
        }
        fclose(fp);
      } else {
        dc_log(DC_LOG_ERROR, "fopen: %s", strerror(errno));
        free(cfg_pathname);
        exit(EXIT_FAILURE);
      }
    } else {
      /* File exists but couldn't be parsed (e.g., empty due to a race between
       * parallel processes). Use defaults so the caller can proceed. */
      dc_log(DC_LOG_ERROR, "Error accessing %s", cfg_pathname);
      for (size_t i = 0; i < player_config_entry_count; i++)
        config_set_from_string_real(&config, &player_config_entries[i],
                                    player_config_entries[i].default_value);
    }
  } else {
    // Track which keys were found
    bool found_keys[MAX_PLAYER_CONFIG_ENTRIES];
    memset(found_keys, 0, sizeof(found_keys));

    while (cfg_node) {
      for (size_t i = 0; i < player_config_entry_count; i++) {
        if (strcasecmp(cfg_node->key, player_config_entries[i].key) == 0) {
          config_set_from_string_real(&config, &player_config_entries[i],
                                      cfg_node->value ? cfg_node->value
                                                      : player_config_entries[i].default_value);
          found_keys[i] = true;
          break;
        }
      }
      canfigger_free_current_key_node_advance(&cfg_node);
    }

    // Append missing keys with default values
    FILE *fp = fopen(cfg_pathname, "a");
    if (!fp) {
      dc_log(DC_LOG_ERROR, "fopen (appending missing keys): %s", strerror(errno));
    } else {
      for (size_t i = 0; i < player_config_entry_count; i++) {
        if (!found_keys[i]) {
          printf("Appending config: %s = %s\n", player_config_entries[i].key,
                 player_config_entries[i].default_value);
          fprintf(fp, "%s = %s\n", player_config_entries[i].key,
                  player_config_entries[i].default_value);
          config_set_from_string_real(&config, &player_config_entries[i],
                                      player_config_entries[i].default_value);
        }
      }
      fclose(fp);
    }
  }

  free(cfg_pathname);

  config.loaded = true;
  return config;
}

LayoutConfig_t get_layout_config(const char *data_dir) {
  LayoutConfig_t config = {0};

  char *cfg_pathname = canfigger_path_join(data_dir, "layout.conf");
  if (!cfg_pathname) {
    dc_log(DC_LOG_ERROR, "canfigger_path_join failed for layout.conf");
    goto use_defaults;
  }

  printf("Reading layout config: %s\n", cfg_pathname);
  /* layout.conf ships with every install, so a missing/unreadable one means a
   * broken or incomplete install — fail loudly rather than silently running with
   * default geometry. (common.conf, by contrast, is optional / LAN-only and only
   * warns.) An existing-but-empty file is fine and falls back to defaults below. */
  FILE *probe = fopen(cfg_pathname, "r");
  if (!probe) {
    dc_log(DC_LOG_ERROR, "cannot open %s (%s); the install looks incomplete (layout.conf is required)",
           cfg_pathname, strerror(errno));
    free(cfg_pathname);
    exit(EXIT_FAILURE);
  }
  fclose(probe);
  struct Canfigger *cfg_node = canfigger_parse_file(cfg_pathname, ',');
  free(cfg_pathname);
  if (!cfg_node)
    goto use_defaults;

  bool found_keys[MAX_LAYOUT_CONFIG_ENTRIES];
  memset(found_keys, 0, sizeof(found_keys));

  while (cfg_node) {
    for (size_t i = 0; i < layout_config_entry_count; i++) {
      if (strcasecmp(cfg_node->key, layout_config_entries[i].key) == 0) {
        config_set_from_string_real(&config, &layout_config_entries[i],
                                    cfg_node->value ? cfg_node->value
                                                    : layout_config_entries[i].default_value);
        found_keys[i] = true;
        break;
      }
    }
    canfigger_free_current_key_node_advance(&cfg_node);
  }

  for (size_t i = 0; i < layout_config_entry_count; i++)
    if (!found_keys[i])
      config_set_from_string_real(&config, &layout_config_entries[i],
                                  layout_config_entries[i].default_value);

  return config;

use_defaults:
  for (size_t i = 0; i < layout_config_entry_count; i++)
    config_set_from_string_real(&config, &layout_config_entries[i],
                                layout_config_entries[i].default_value);
  return config;
}
