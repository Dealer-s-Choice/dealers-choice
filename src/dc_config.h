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

#define MAX_REGISTRIES 4
#define REGISTRY_HOST_LEN 64

typedef struct ServerConfig_t {
  char bind_address[TCPME_ADDRSTRLEN];
  uint16_t port;
  char server_name[64]; /* advertised to LAN discovery + the registry (#33) */
  char registry_host[MAX_REGISTRIES][REGISTRY_HOST_LEN]; /* registries to publish to */
  uint16_t registry_port[MAX_REGISTRIES];
  uint8_t registry_count;
  uint32_t end_of_game_timeout_ms;
  uint32_t action_timeout_ms;
  uint32_t dealer_timeout_ms;
  uint32_t ante;
  uint32_t bringin_amount;
  int32_t starting_coins;
  uint32_t max_raises;
  uint32_t bet_amounts[MAX_BET_AMOUNTS];
  uint8_t bet_amount_count;
  uint8_t action_timeout_max;
  char password[256];
  uint32_t max_connections_per_minute;
  uint32_t max_connections_per_ip;
} ServerConfig_t;

#define SIZEOF_HOTKEY_NAME 16

typedef struct {
  bool loaded;
  char nick[SIZEOF_NICK];
  char host[MAX_INPUT_LENGTH];
  uint16_t port;
  char language[6];
  int volume;
  bool turn_notify;
  uint8_t connect_attempts;
  char password[MAX_INPUT_LENGTH];
  char hotkey_check[SIZEOF_HOTKEY_NAME];
  char hotkey_bet[SIZEOF_HOTKEY_NAME];
  char hotkey_fold[SIZEOF_HOTKEY_NAME];
  char hotkey_call[SIZEOF_HOTKEY_NAME];
  char hotkey_raise[SIZEOF_HOTKEY_NAME];
  char hotkey_complete[SIZEOF_HOTKEY_NAME];
  char hotkey_discard[SIZEOF_HOTKEY_NAME];
  char hotkey_hand_rank[SIZEOF_HOTKEY_NAME];
  char registry_host[MAX_REGISTRIES][REGISTRY_HOST_LEN]; /* registries to browse (#33) */
  uint16_t registry_port[MAX_REGISTRIES];
  uint8_t registry_count;
} PlayerConfig_t;

/* Parse a "host" / "host:port" spec and append to a registry list. */
void registry_list_add(char host_out[][REGISTRY_HOST_LEN], uint16_t *port_out, uint8_t *count,
                       const char *spec);

/* Read the shared registry list from <data_dir>/common.conf (used by both the
 * server, to publish, and the client, to browse). */
void get_common_registries(const char *data_dir, char host[][REGISTRY_HOST_LEN], uint16_t *port,
                           uint8_t *count);

typedef enum {
  CFG_TYPE_STRING,
  CFG_TYPE_INT,
  CFG_TYPE_UINT8,
  CFG_TYPE_UINT16,
  CFG_TYPE_UINT32,
  CFG_TYPE_BOOL
} ConfigType;

typedef struct {
  const char *key;
  ConfigType type;
  const char *default_value; // Stored as string
  size_t offset;             // Offset into PlayerConfig_t
  size_t size;               // Size of the target field
} ConfigEntry;

#define MAX_PLAYER_CONFIG_ENTRIES 24
#define MAX_SERVER_CONFIG_ENTRIES 32
#define MAX_LAYOUT_CONFIG_ENTRIES 80

extern const ConfigEntry player_config_entries[];
extern const ConfigEntry server_config_entries[];
extern const ConfigEntry layout_config_entries[];

extern const size_t player_config_entry_count;
extern const size_t server_config_entry_count;
extern const size_t layout_config_entry_count;

ServerConfig_t get_server_config(Path_t *path, const CliArgs_t *cli_args);

PlayerConfig_t get_player_config(void);

void player_config_set_field(PlayerConfig_t *cfg, size_t entry_idx, const char *val);

void save_player_config(const PlayerConfig_t *config);

#endif
