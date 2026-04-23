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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#include "dc_config.h"
#include "util.h"

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
    {0}};

const ConfigEntry server_config_entries[] = {
    {"bind_address", CFG_TYPE_STRING, "127.0.0.1", offsetof(ServerConfig_t, bind_address),
     sizeof(((ServerConfig_t *)0)->bind_address)},
    {"port", CFG_TYPE_UINT16, DEFAULT_PORT, offsetof(ServerConfig_t, port), sizeof(uint16_t)},
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
    fprintf(stderr, "Unable to determine config directory.\n");
    return;
  }

  char *cfg_pathname = canfigger_path_join(cfgdir, "player.conf");
  free(cfgdir);
  if (!cfg_pathname) {
    fprintf(stderr, "canfigger_path_join failed\n");
    return;
  }

  FILE *fp = fopen(cfg_pathname, "w");
  if (!fp) {
    perror("fopen");
    free(cfg_pathname);
    return;
  }

  char val_str[MAX_INPUT_LENGTH];
  for (size_t i = 0; i < player_config_entry_count; i++) {
    config_field_to_string(config, &player_config_entries[i], val_str, sizeof(val_str));
    fprintf(fp, "%s = %s\n", player_config_entries[i].key, val_str);
  }
  fclose(fp);
  free(cfg_pathname);
}

void player_config_set_field(PlayerConfig_t *cfg, size_t entry_idx, const char *val) {
  if (entry_idx >= player_config_entry_count)
    return;
  config_set_from_string_real(cfg, &player_config_entries[entry_idx], val);
}

ServerConfig_t get_server_config(Path_t *path, const CliArgs_t *cli_args) {
  ServerConfig_t config = {0};

  if (!cli_args->server_conf) {
    path->server_conf_name = canfigger_path_join(path->data, "server.conf");
    if (!path->server_conf_name) {
      fprintf(stderr, "canfigger_path_join failed\n");
      return config;
    }
  } else {
    path->server_conf_name = dc_strdup(cli_args->server_conf);
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

  return config;
}

PlayerConfig_t get_player_config(void) {
  PlayerConfig_t config = {0};
  config.loaded = false;

  char *cfgdir = canfigger_config_dir(DEALERSCHOICE_NAME);
  if (!cfgdir) {
    fprintf(stderr, "Unable to determine config directory.\n");
    return config;
  }

  EPathState state = check_pathname_state(cfgdir);
  if (state == PATH_NOT_FOUND) {
    if (make_directory_recursive(cfgdir) != 0) {
      fprintf(stderr, "Failed to create config dir: %s\n", cfgdir);
      free(cfgdir);
      return config;
    }
  } else if (state == PATH_ERROR) {
    fprintf(stderr, "Error checking config dir: %s\n", cfgdir);
    free(cfgdir);
    return config;
  }

  char *cfg_pathname = canfigger_path_join(cfgdir, "player.conf");
  free(cfgdir);
  if (!cfg_pathname) {
    fprintf(stderr, "canfigger_path_join failed\n");
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
        perror("fopen");
        free(cfg_pathname);
        exit(EXIT_FAILURE);
      }
    } else {
      /* File exists but couldn't be parsed (e.g., empty due to a race between
       * parallel processes). Use defaults so the caller can proceed. */
      fprintf(stderr, "Error accessing %s\n", cfg_pathname);
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
      perror("fopen (appending missing keys)");
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
