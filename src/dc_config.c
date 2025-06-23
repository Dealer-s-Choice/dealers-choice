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

// Required to define some macros when including limits.h on Alpine Linux
#define _XOPEN_SOURCE

#include <canfigger.h>
#include <limits.h> // For PATH_MAX (TODO: Use pathconf() instead)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dc_config.h"

Config_t get_config(Path_t *path, CliArgs_t *cli_args) {
  if (!cli_args->server_conf)
    snprintf(path->server_conf_name, sizeof(path->server_conf_name), "%s/%s", path->data,
             "server.conf");
  else
    snprintf(path->server_conf_name, sizeof(path->server_conf_name), "%s", cli_args->server_conf);

  struct Canfigger *cfg_node = canfigger_parse_file(path->server_conf_name, ',');
  if (!cfg_node) {
    perror("canfigger");
    exit(EXIT_FAILURE);
  }

  Config_t config = {0};

  // Adapted from rmw (https://github.com/theimpossibleastronaut/rmw).
  //
  // Jammy should improve the variable names before the first release ;)
  typedef enum {
    BIND_ADDRESS,
    END_OF_ROUND_TIMEOUT_MS,
    ACTION_TIMEOUT_MS,
    INVALID_OPTION,
  } cfg_opt_id;

  struct Opt_t {
    const char *opt;
    cfg_opt_id id;
  };

  struct Opt_t opt[] = {{"bind_address", BIND_ADDRESS},
                        {"end_of_game_timeout_ms", END_OF_ROUND_TIMEOUT_MS},
                        {"action_timeout_ms", ACTION_TIMEOUT_MS},
                        {NULL, INVALID_OPTION}};

  int cfg_idx = 0;
  while (cfg_node != NULL) {
    if (strcmp(cfg_node->key, opt[cfg_idx].opt) != 0) {
      fprintf(stderr, "Invalid option: %s\n", cfg_node->key);
      exit(EXIT_FAILURE);
      break;
    }
    switch (opt[cfg_idx].id) {
    case BIND_ADDRESS:
      snprintf(config.bind_address, sizeof(config.bind_address), "%s", cfg_node->value);
      break;
    case END_OF_ROUND_TIMEOUT_MS:
      config.end_of_game_timeout_ms = (uint32_t)strtol(cfg_node->value, NULL, 0);
      break;
    case ACTION_TIMEOUT_MS:
      config.action_timeout_ms = (uint32_t)strtol(cfg_node->value, NULL, 0);
      break;
    default:
      break;
    }
    cfg_idx++;
    canfigger_free_current_key_node_advance(&cfg_node);
  }
  return config;
}

static void config_set_from_string(PlayerConfig_t *cfg, const ConfigEntry *entry, const char *val) {
  void *field = (uint8_t *)cfg + entry->offset;

  switch (entry->type) {
  case CFG_TYPE_STRING:
    snprintf((char *)field, entry->size, "%s", val);
    break;
  case CFG_TYPE_INT:
    *(int *)field = atoi(val);
    break;
  }
}

static void config_set_default(PlayerConfig_t *cfg, const ConfigEntry *entry) {
  config_set_from_string(cfg, entry, entry->default_value);
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

  char cfg_pathname[PATH_MAX];
  snprintf(cfg_pathname, sizeof(cfg_pathname), "%s/player.conf", cfgdir);
  free(cfgdir);

  printf("Reading config: %s\n", cfg_pathname);
  struct Canfigger *cfg_node = canfigger_parse_file(cfg_pathname, ',');
  if (!cfg_node) {
    if (check_pathname_state(cfg_pathname) == PATH_NOT_FOUND) {
      printf("Creating %s\n", cfg_pathname);
      FILE *fp = fopen(cfg_pathname, "w");
      if (fp) {
        for (size_t i = 0; i < config_entry_count; i++) {
          fprintf(fp, "%s = %s\n", config_entries[i].key, config_entries[i].default_value);
          config_set_default(&config, &config_entries[i]);
        }
        fclose(fp);
      } else {
        perror("fopen");
        exit(EXIT_FAILURE);
      }
    } else {
      fprintf(stderr, "Error accessing %s\n", cfg_pathname);
      exit(EXIT_FAILURE);
    }
  } else {
    while (cfg_node) {
      for (size_t i = 0; i < config_entry_count; i++) {
        if (strcasecmp(cfg_node->key, config_entries[i].key) == 0) {
          config_set_from_string(&config, &config_entries[i], cfg_node->value);
          break;
        }
      }
      canfigger_free_current_key_node_advance(&cfg_node);
    }
  }

  config.loaded = true;
  return config;
}
