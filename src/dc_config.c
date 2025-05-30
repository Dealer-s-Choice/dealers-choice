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
    END_OF_ROUND_TIMEOUT_MS,
    ACTION_TIMEOUT_MS,
    INVALID_OPTION,
  } cfg_opt_id;

  struct Opt_t {
    const char *opt;
    cfg_opt_id id;
  };

  struct Opt_t opt[] = {{"end_of_round_time_out_ms", END_OF_ROUND_TIMEOUT_MS},
                        {"action_time_out_ms", ACTION_TIMEOUT_MS},
                        {NULL, INVALID_OPTION}};

  int cfg_idx = 0;
  while (cfg_node != NULL) {
    switch (opt[cfg_idx].id) {
    case END_OF_ROUND_TIMEOUT_MS:
      config.end_of_round_time_out_ms = atoi(cfg_node->value);
      break;
    case ACTION_TIMEOUT_MS:
      config.action_time_out_ms = atoi(cfg_node->value);
      break;

    default:
    case INVALID_OPTION:
      fprintf(stderr, "Invalid option: %s\n", cfg_node->key);
    }
    canfigger_free_current_key_node_advance(&cfg_node);
  }
  return config;
}
