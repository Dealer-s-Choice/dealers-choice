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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "net.h"

const char *default_port = "61357";

void close_socket_checked(socket_t sockfd) {
#ifdef _WIN32
  if (closesocket(sockfd) != 0)
    perror("closesocket");
#else
  if (close(sockfd) != 0)
    perror("close");
#endif
}

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

ssize_t send_all(int sockfd, const void *buf, size_t len) {
  size_t total_sent = 0;
  const char *p = (const char *)buf;

  while (total_sent < len) {
    ssize_t n = send(sockfd, p + total_sent, len - total_sent, 0);
    if (n <= 0)
      return -1; // Error or disconnect
    total_sent += n;
  }
  return total_sent;
}

int recv_all(int sock, void *buffer, size_t length) {
  size_t received = 0;
  while (received < length) {
    ssize_t ret = recv(sock, (char *)buffer + received, length - received, 0);
    if (ret <= 0)
      return -1;
    received += ret;
  }
  return 0;
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
