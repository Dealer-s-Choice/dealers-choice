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

static void config_set_from_string_real(void *cfg, const ConfigEntry *entry, const char *val) {
  void *field = (uint8_t *)cfg + entry->offset;

  switch (entry->type) {
  case CFG_TYPE_STRING:
    snprintf((char *)field, entry->size, "%s", val);
    break;
  case CFG_TYPE_INT:
    *(int *)field = atoi(val);
    break;
  case CFG_TYPE_UINT32:
    *(uint32_t *)field = strtol(val, NULL, 0);
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

static void server_config_set_from_string(ServerConfig_t *cfg, const ConfigEntry *entry,
                                          const char *val) {
  config_set_from_string_real(cfg, entry, val);
}

static void player_config_set_from_string(PlayerConfig_t *cfg, const ConfigEntry *entry,
                                          const char *val) {
  config_set_from_string_real(cfg, entry, val);
}

ServerConfig_t get_server_config(Path_t *path, CliArgs_t *cli_args) {
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
  bool found_keys[config_entry_count];
  memset(found_keys, 0, sizeof(found_keys));

  while (cfg_node) {
    for (size_t i = 0; i < config_entry_count; i++) {
      if (strcasecmp(cfg_node->key, server_config_entries[i].key) == 0) {
        server_config_set_from_string(&config, &server_config_entries[i], cfg_node->value);
        found_keys[i] = true;
        break;
      }
    }
    canfigger_free_current_key_node_advance(&cfg_node);
  }
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
        for (size_t i = 0; i < config_entry_count; i++) {
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
      fprintf(stderr, "Error accessing %s\n", cfg_pathname);
      free(cfg_pathname);
      exit(EXIT_FAILURE);
    }
  } else {
    // Track which keys were found
    bool found_keys[config_entry_count];
    memset(found_keys, 0, sizeof(found_keys));

    while (cfg_node) {
      for (size_t i = 0; i < config_entry_count; i++) {
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
      for (size_t i = 0; i < config_entry_count; i++) {
        if (!found_keys[i]) {
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
