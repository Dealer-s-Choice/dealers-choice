/*
 dc_config.h
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

#ifndef __DC_CONFIG_H
#define __DC_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "net.h"
#include "util.h"

typedef struct {
  char bind_address[INET6_ADDRSTRLEN];
  uint32_t end_of_game_timeout_ms;
  uint32_t action_timeout_ms;
  uint32_t dealer_timeout_ms;
} ServerConfig_t;

typedef struct {
  bool loaded;
  char nick[SIZEOF_NICK];
  char host[MAX_INPUT_LENGTH];
  int port;
  int volume;
  bool turn_notify;
} PlayerConfig_t;

typedef enum { CFG_TYPE_STRING, CFG_TYPE_INT, CFG_TYPE_UINT32, CFG_TYPE_BOOL } ConfigType;

typedef struct {
  const char *key;
  ConfigType type;
  const char *default_value; // Stored as string
  size_t offset;             // Offset into PlayerConfig_t
  size_t size;               // Size of the target field
} ConfigEntry;

static const ConfigEntry player_config_entries[] = {
    {"nick", CFG_TYPE_STRING, "New Player", offsetof(PlayerConfig_t, nick),
     sizeof(((PlayerConfig_t *)0)->nick)},
    {"host", CFG_TYPE_STRING, "127.0.0.1", offsetof(PlayerConfig_t, host),
     sizeof(((PlayerConfig_t *)0)->host)},
    {"port", CFG_TYPE_INT, DEFAULT_PORT, offsetof(PlayerConfig_t, port), sizeof(int)},
    {"sound.volume", CFG_TYPE_INT, "5", offsetof(PlayerConfig_t, volume), sizeof(int)},
    {"sound.notify.turn", CFG_TYPE_BOOL, "yes", offsetof(PlayerConfig_t, turn_notify),
     sizeof(bool)}};

static const size_t config_entry_count =
    sizeof(player_config_entries) / sizeof(player_config_entries[0]);

static const ConfigEntry server_config_entries[] = {
    {"bind_address", CFG_TYPE_STRING, NULL, offsetof(ServerConfig_t, bind_address),
     sizeof(((ServerConfig_t *)0)->bind_address)},
    {"end_of_game_timeout_ms", CFG_TYPE_UINT32, NULL,
     offsetof(ServerConfig_t, end_of_game_timeout_ms), sizeof(uint32_t)},
    {"action_timeout_ms", CFG_TYPE_UINT32, NULL, offsetof(ServerConfig_t, action_timeout_ms),
     sizeof(uint32_t)},
    {"dealer_timeout_ms", CFG_TYPE_UINT32, NULL, offsetof(ServerConfig_t, dealer_timeout_ms),
     sizeof(uint32_t)}};

ServerConfig_t get_server_config(Path_t *path, CliArgs_t *cli_args);

PlayerConfig_t get_player_config(void);

#endif
