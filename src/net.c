/*
 net.c
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

#include "net.h"

const char *default_port = "61357";

uint8_t *serialize_player(const struct player_t *src, size_t *size_out) {
  Player msg = PLAYER__INIT;
  Pos pos = POS__INIT;
  Hand hand = HAND__INIT;
  Card cards[HAND_SIZE];
  Card *card_ptrs[HAND_SIZE];

  // Fill in name
  msg.name = (char *)src->name;

  // Fill in id
  msg.id = src->id;

  // Fill in position
  pos.x = src->pos.x;
  pos.y = src->pos.y;
  msg.pos = &pos;

  // Fill in cards
  for (int i = 0; i < HAND_SIZE; ++i) {
    card__init(&cards[i]);
    cards[i].face_val = src->hand.card[i].face_val;
    cards[i].suit = src->hand.card[i].suit;
    card_ptrs[i] = &cards[i]; // pointer to each card
  }

  hand.n_card = HAND_SIZE;
  hand.card = card_ptrs;
  msg.hand = &hand;

  // Fill in chips
  msg.chips = src->chips;

  // Serialize to buffer
  *size_out = player__get_packed_size(&msg);
  uint8_t *buffer = malloc(*size_out);
  if (!buffer) {
    *size_out = 0;
    return NULL;
  }

  player__pack(&msg, buffer);
  return buffer;
}

struct player_t deserialize_player(const uint8_t *data, size_t size) {
  struct player_t player = {0};
  Player *pb = player__unpack(NULL, size, data);
  if (!pb) {
    fprintf(stderr, "Failed to unpack Player\n");
    return player;
  }

  strncpy(player.name, pb->name, sizeof(player.name) - 1);
  player.id = pb->id;

  if (pb->pos) {
    player.pos.x = pb->pos->x;
    player.pos.y = pb->pos->y;
  }

  if (pb->hand && pb->hand->n_card <= HAND_SIZE) {
    for (size_t i = 0; i < pb->hand->n_card; i++) {
      player.hand.card[i].face_val = pb->hand->card[i]->face_val;
      player.hand.card[i].suit = pb->hand->card[i]->suit;
    }
  }

  player.chips = pb->chips;

  player__free_unpacked(pb, NULL);
  return player;
}

/**
 * Sends a block of data reliably over a TCP socket.
 *
 * @param sock The TCP socket to send on.
 * @param data Pointer to the data buffer to send.
 * @param length Number of bytes to send.
 * @return 0 on success, -1 on failure.
 */
int send_all_tcp(TCPsocket sock, const void *data, size_t length) {
  const uint8_t *buf = (const uint8_t *)data;
  size_t total_sent = 0;

  while (total_sent < length) {
    int sent = SDLNet_TCP_Send(sock, buf + total_sent, (int)(length - total_sent));
    if (sent <= 0) {
      fprintf(stderr, "SDLNet_TCP_Send failed: %s\n", SDLNet_GetError());
      return -1;
    }
    total_sent += sent;
  }

  return 0;
}

int recv_all_tcp(TCPsocket sock, void *data, size_t length) {
  uint8_t *buf = (uint8_t *)data;
  size_t total_received = 0;

  while (total_received < length) {
    int received = SDLNet_TCP_Recv(sock, buf + total_received, (int)(length - total_received));
    if (received <= 0) {
      fprintf(stderr, "SDLNet_TCP_Recv failed or connection closed: %s\n", SDLNet_GetError());
      return -1;
    }
    total_received += received;
  }

  return 0;
}
