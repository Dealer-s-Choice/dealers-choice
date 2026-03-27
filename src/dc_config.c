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

#include "dc_config.h"
#include "util.h"

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
  char *cfgdir = get_config_dir();
  if (!cfgdir) {
    fprintf(stderr, "Unable to determine config directory.\n");
    return;
  }

  PathconfLimits_t limits = {0};
  get_pathconf_limits(cfgdir, &limits);
  char *cfg_pathname = calloc_wrap(limits.path_max, 1);
  snprintf(cfg_pathname, limits.path_max, "%s/player.conf", cfgdir);
  free(cfgdir);

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

static void server_config_set_from_string(ServerConfig_t *cfg, const ConfigEntry *entry,
                                          const char *val) {
  config_set_from_string_real(cfg, entry, val);
}

static void player_config_set_from_string(PlayerConfig_t *cfg, const ConfigEntry *entry,
                                          const char *val) {
  config_set_from_string_real(cfg, entry, val);
}

void player_config_set_field(PlayerConfig_t *cfg, size_t entry_idx, const char *val) {
  if (entry_idx >= player_config_entry_count)
    return;
  player_config_set_from_string(cfg, &player_config_entries[entry_idx], val);
}

ServerConfig_t get_server_config(Path_t *path, const CliArgs_t *cli_args) {
  ServerConfig_t config = {0};

  PathconfLimits_t limits = {0};
  if (!cli_args->server_conf) {
    get_pathconf_limits(path->data, &limits);
    path->server_conf_name = calloc_wrap(limits.path_max, 1);
    snprintf(path->server_conf_name, limits.path_max, "%s/%s", path->data, "server.conf");
  } else {
    get_pathconf_limits(cli_args->server_conf, &limits);
    path->server_conf_name = calloc_wrap(limits.path_max, 1);
    snprintf(path->server_conf_name, limits.path_max, "%s", cli_args->server_conf);
  }

  printf("Reading server config: %s\n", path->server_conf_name);
  struct Canfigger *cfg_node = canfigger_parse_file(path->server_conf_name, ',');
  free(path->server_conf_name);
  if (!cfg_node) {
    perror("canfigger");
    exit(EXIT_FAILURE);
  }

  // Track which keys were found
  bool found_keys[server_config_entry_count];
  memset(found_keys, 0, sizeof(found_keys));

  while (cfg_node) {
    for (size_t i = 0; i < server_config_entry_count; i++) {
      if (strcasecmp(cfg_node->key, server_config_entries[i].key) == 0) {
        server_config_set_from_string(&config, &server_config_entries[i], cfg_node->value);
        found_keys[i] = true;
        break;
      }
    }
    for (size_t i = 0; i < server_config_entry_count; i++)
      if (!found_keys[i])
        server_config_set_from_string(&config, &server_config_entries[i],
                                      server_config_entries[i].default_value);

    canfigger_free_current_key_node_advance(&cfg_node);
  }

  // DC_PASSWORD env var takes precedence over server.conf
  const char *env_pw = getenv("DC_PASSWORD");
  if (env_pw)
    snprintf(config.password, sizeof(config.password), "%s", env_pw);

  return config;
}

static void config_set_default(PlayerConfig_t *cfg, const ConfigEntry *entry) {
  player_config_set_from_string(cfg, entry, entry->default_value);
}

PlayerConfig_t get_player_config(void) {
  PlayerConfig_t config = {0};
  config.loaded = false;

  char *cfgdir = get_config_dir(); // your cross-platform config dir resolver
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

  PathconfLimits_t limits = {0};
  get_pathconf_limits(cfgdir, &limits);
  char *cfg_pathname = calloc_wrap(limits.path_max, 1);
  snprintf(cfg_pathname, limits.path_max, "%s/player.conf", cfgdir);
  free(cfgdir);

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
          config_set_default(&config, &player_config_entries[i]);
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
        config_set_default(&config, &player_config_entries[i]);
    }
  } else {
    // Track which keys were found
    bool found_keys[player_config_entry_count];
    memset(found_keys, 0, sizeof(found_keys));

    while (cfg_node) {
      for (size_t i = 0; i < player_config_entry_count; i++) {
        if (strcasecmp(cfg_node->key, player_config_entries[i].key) == 0) {
          player_config_set_from_string(&config, &player_config_entries[i], cfg_node->value);
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
          config_set_default(&config, &player_config_entries[i]);
        }
      }
      fclose(fp);
    }
  }

  free(cfg_pathname);

  config.loaded = true;
  return config;
}
