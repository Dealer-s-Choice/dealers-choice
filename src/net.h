/*
 net.h
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

#ifndef __NET_H
#define __NET_H

#include <SDL2/SDL_net.h>

// htonl and ntohl
#ifdef _WIN32
#include "dc_windows.h"
#else
#include <arpa/inet.h>
#endif

#include "netpoker.pb-c.h"
#include "types.h"

#ifndef MAX_HAND_SIZE
#define MAX_HAND_SIZE 7
#endif

#define GAME_PROTOCOL_MAGIC "DCPROTO"
#define GAME_PROTOCOL_VERSION 3

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

typedef struct {
  char magic[sizeof(GAME_PROTOCOL_MAGIC)];
  uint32_t version; // Network byte order
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

#define MSG_GAME_SELECT 0x0001       // Player_t chooses a game variant
#define MSG_PLAYER_ACTION 0x0002     // Player_t bets, folds, etc.
#define MSG_GAME_START 0x0004        // Game begins
#define MSG_DEAL_CARDS 0x0005        // Cards sent to player
#define MSG_DRAW_REQUEST 0x0006      // Player_t discards cards for draw
#define MSG_GAME_STATE_UPDATE 0x0007 // Server sends state update
#define MSG_DRAW_PROMPT 0x0008
#define MSG_STATUS_MESSAGE 0x0009
#define MSG_NEW_HAND 0x0010
#define MSG_START_ACTION_TIMER 0x0011

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
  bool do_discard_draw;
  bool has_ace;
  uint8_t n_cards_selected;
  int selected_amount;
  bool cards_sent;
  int8_t save_starting_turn_id;
  uint32_t timer_start;
  char server_status_str[LEN_STATUS_STR];
  bool play_coin_sound;
} ClientState_t;

struct player_message_builder_t {
  // These types come from the generated protobuf header file
  Player msg;
  Hand hand;
  Card cards[MAX_HAND_SIZE];
  Card *card_ptrs[MAX_HAND_SIZE];
};

uint8_t *serialize_game_state(const GameState_t *src, size_t *size_out);
GameState_t deserialize_game_state(const uint8_t *data, size_t size);

uint8_t *serialize_game_settings(const GameSettings_t *src, size_t *size_out);
GameSettings_t deserialize_game_settings(const uint8_t *data, size_t size);

uint8_t *serialize_player(const Player_t *src, size_t *size_out);
Player_t deserialize_player(const uint8_t *data, size_t size);

int send_all_tcp(TCPsocket sock, const void *data, size_t length);

int recv_all_tcp(TCPsocket sock, void *data, int32_t length);

ERecvStatus_t recv_game_state(SocketContext_t *socket_context, GameState_t *game_state,
                              ClientState_t *client_state, const int8_t id);

ERecvStatus_t recv_game_settings(TCPsocket client_socket, SDLNet_SocketSet socket_set,
                                 GameSettings_t *game_settings);

void socket_cleanup(SocketContext_t *socket_context);

#endif
