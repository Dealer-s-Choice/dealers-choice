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

Config_t get_config(Path_t *path) {

  snprintf(path->server_conf_name, sizeof(path->server_conf_name), "%s/%s", path->data,
           "server.conf");
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
                        {"end_of_round_time_out_ms", END_OF_ROUND_TIMEOUT_MS},
                        {"action_time_out_ms", ACTION_TIMEOUT_MS},
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
      config.end_of_round_time_out_ms = atoi(cfg_node->value);
      break;
    case ACTION_TIMEOUT_MS:
      config.action_time_out_ms = atoi(cfg_node->value);
      break;
    default:
      break;
    }
    cfg_idx++;
    canfigger_free_current_key_node_advance(&cfg_node);
  }
  return config;
}

PlayerConfig_t get_player_config(void) {
  enum {
    NICK,
    MAX_KEYS,
  };

  const char *key[] = {[NICK] = "nick", [MAX_KEYS] = NULL};

  PlayerConfig_t config = {0};
  config.loaded = false;
  char *cfgdir = get_config_dir();
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

  // Now cfgdir points to a usable config directory
  printf("Using config dir: %s\n", cfgdir);

  // TODO: Use pathconf() instead, and also check NAME_MAX
  char cfg_pathname[PATH_MAX];
  snprintf(cfg_pathname, sizeof(cfg_pathname), "%s/player.conf", cfgdir);
  free(cfgdir);
  struct Canfigger *cfg_node = canfigger_parse_file(cfg_pathname, ',');
  if (!cfg_node) {
    if (check_pathname_state(cfg_pathname) == PATH_NOT_FOUND) {
      FILE *fp = fopen(cfg_pathname, "w");
      if (fp) {
        snprintf(config.nick, sizeof(config.nick), "New Player");
        fprintf(fp, "%s = New Player\n", key[NICK]);
        fclose(fp);
      } else {
        perror("fopen");
        exit(EXIT_FAILURE);
      }
    } else {
      fprintf(stderr, "Error when trying to access %s\n", cfg_pathname);
      exit(EXIT_FAILURE);
    }
  }

  int cfg_idx = 0;
  while (cfg_node != NULL) {
    if (strcmp(cfg_node->key, key[cfg_idx]) != 0) {
      fprintf(stderr, "Invalid option: %s\n", cfg_node->key);
      exit(EXIT_FAILURE);
      break;
    }
    switch (cfg_idx) {
    case NICK:
      snprintf(config.nick, sizeof(config.nick), "%s", cfg_node->value);
      break;
    default:
      break;
    }
    cfg_idx++;
    canfigger_free_current_key_node_advance(&cfg_node);
  }
  config.loaded = true;
  return config;
}
