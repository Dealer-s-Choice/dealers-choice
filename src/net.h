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
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "netpoker.pb-c.h"
#include "types.h"

#define BACKLOG 10

extern const char *default_port;

struct player_message_builder_t {
  // These types come from the generated protobuf header file
  Player msg;
  Pos pos;
  Hand hand;
  Card cards[HAND_SIZE];
  Card *card_ptrs[HAND_SIZE];
};

uint8_t *serialize_player(const struct player_t *src, size_t *size_out);

struct player_t deserialize_player(const uint8_t *data, size_t size);

int send_all_tcp(TCPsocket sock, const void *data, size_t length);

int recv_all_tcp(TCPsocket sock, void *data, size_t length);

#endif
