/*
 net.h
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025,2026 Andy Alt

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

#ifndef __NET_H
#define __NET_H

#include <SDL2/SDL_net.h>

#include "globals.h"
#include "netpoker.pb-c.h"
#include "types.h"

#define NONCE_SIZE 32
#define HASH_SIZE 32 /* SHA-256 output size, matches crypto_hash_sha256_BYTES */

#ifndef MAX_HAND_SIZE
#define MAX_HAND_SIZE 7
#endif

#define GAME_PROTOCOL_MAGIC "DCPROTO"
#define GAME_PROTOCOL_VERSION 7

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

typedef struct {
  char magic[sizeof(GAME_PROTOCOL_MAGIC)];
  uint16_t version; // Network byte order
}
#ifdef _MSC_VER
GameProtocolHeader_t;
#pragma pack(pop)
#else
__attribute__((packed)) GameProtocolHeader_t;
#endif

// On Windows, this is defined in <ws2tcpip.h>. Rather than include the file
// let's just do this...
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#define OPCODE_SIZE sizeof(uint16_t)
#define LENGTH_PREFIX_SIZE sizeof(uint32_t)

#define MSG_GAME_SELECT 0x0001   // Player_t chooses a game variant
#define MSG_PLAYER_ACTION 0x0002 // Player_t bets, folds, etc.
#define MSG_PING_BROADCAST 0x0003
#define MSG_PING_REQUEST 0x0004
#define MSG_PING_RESPONSE 0x0005
#define MSG_DEAL_CARDS 0x0006   // Cards sent to player
#define MSG_DRAW_REQUEST 0x0007 // Player_t discards cards for draw
#define MSG_WILD_REPLACEMENT 0x0008
#define MSG_DRAW_PROMPT 0x0009
#define MSG_STATUS_MESSAGE 0x000A
#define MSG_NEW_HAND 0x000B
#define MSG_BET_CHECK_FOLD 0x000C
#define MSG_CALL_RAISE_FOLD 0x000D
#define MSG_TURN_ID 0x000E
#define MSG_KICK_PLAYER 0x000F // Admin kicks a player
#define MSG_BAN_PLAYER 0x0010  // Admin bans a player by IP

#define DEFAULT_PORT "22777"

typedef enum {
  RECV_ERROR,
  RECV_SUCCESS,
  RECV_NOTHING,
} ERecvStatus_t;

typedef struct {
  TCPsocket sock;
  SDLNet_SocketSet set;
} SocketContext_t;

typedef struct {
  int8_t turn_id;
  bool turn_switch;
  bool do_discard_draw;
  bool do_exchange_wilds;
  bool has_ace;
  uint8_t n_cards_selected;
  uint32_t selected_amount;
  uint32_t timer_start;
  bool end_game_timer_set;
  char server_status_str[LEN_STATUS_STR];
  bool play_coin_sound;
  bool bet_check_fold;
  bool call_raise_fold;
  unsigned int ping_times[MAX_CLIENTS];
  uint8_t game_type;
  bool deuces_wild;
  const GameChoice_t *game_choice;
} ClientState_t;

struct player_message_builder_t {
  // These types come from the generated protobuf header file
  Player msg;
  Hand hand;
  Card cards[MAX_HAND_SIZE];
  Card *card_ptrs[MAX_HAND_SIZE];
};

uint8_t *serialize_game_state(const GameState_t *src, uint32_t *size_out);
GameState_t deserialize_game_state(const uint8_t *data, uint32_t size);

uint8_t *serialize_game_settings(const GameSettings_t *src, size_t *size_out);
GameSettings_t deserialize_game_settings(const uint8_t *data, size_t size);

uint8_t *serialize_hand(const POKEVAL_Hand_7 hand, size_t *size_out);
POKEVAL_Hand_7 deserialize_hand(const uint8_t *data, size_t size);

uint8_t *serialize_player(const Player_t *src, size_t *size_out);
Player_t deserialize_player(const uint8_t *data, size_t size);

int send_all_tcp(TCPsocket sock, const void *data, size_t length);

int recv_all_tcp(TCPsocket sock, void *buf, size_t len);

ERecvStatus_t recv_game_state(SocketContext_t *socket_context, GameState_t *game_state,
                              ClientState_t *client_state, const int8_t id);

ERecvStatus_t recv_game_settings(TCPsocket client_socket, SDLNet_SocketSet socket_set,
                                 GameSettings_t *game_settings);

void socket_cleanup(SocketContext_t *socket_context);

int send_message(TCPsocket sock, uint16_t opcode, const uint8_t *payload, size_t payload_len);

#endif
