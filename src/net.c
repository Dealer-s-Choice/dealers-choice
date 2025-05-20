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

const uint16_t default_port = 22777;

static void fill_player_message(struct player_message_builder_t *builder,
                                const struct player_t *src) {
  player__init(&builder->msg);
  pos__init(&builder->pos);
  hand__init(&builder->hand);

  // Name
  builder->msg.name = (char *)src->name;

  // ID and Chips
  builder->msg.id = src->id;
  builder->msg.coins = src->coins;
  builder->msg.in = src->in;
  builder->msg.total_paid = src->total_paid;
  builder->msg.winner = src->winner;
  builder->msg.has_checked = src->has_checked;

  // Hand
  for (int i = 0; i < HAND_SIZE; ++i) {
    card__init(&builder->cards[i]);
    builder->cards[i].face_val = src->hand.card[i].face_val;
    builder->cards[i].suit = src->hand.card[i].suit;
    builder->card_ptrs[i] = &builder->cards[i];
  }

  builder->hand.n_card = HAND_SIZE;
  builder->hand.card = builder->card_ptrs;
  builder->msg.hand = &builder->hand;
}

static void fill_player_from_message(struct player_t *dst, const Player *msg) {
  if (!msg)
    return;

  if (msg->name)
    snprintf(dst->name, sizeof(dst->name), "%s", msg->name);

  dst->id = msg->id;
  dst->coins = msg->coins;
  dst->in = msg->in;
  dst->total_paid = msg->total_paid;
  dst->winner = msg->winner;
  dst->has_checked = msg->has_checked;

  if (msg->hand) {
    size_t n = msg->hand->n_card < HAND_SIZE ? msg->hand->n_card : HAND_SIZE;
    for (size_t i = 0; i < n; ++i) {
      dst->hand.card[i].face_val = msg->hand->card[i]->face_val;
      dst->hand.card[i].suit = msg->hand->card[i]->suit;
    }
  }
}

uint8_t *serialize_game_state(const game_state_t *src, size_t *size_out) {
  GameState msg = GAME_STATE__INIT;

  // Pot
  msg.pot = src->pot;
  msg.dealer_id = src->dealer_id;
  msg.turn_id = src->turn_id;
  msg.at_menu = src->at_menu;
  msg.total_bets_plus_raises = src->total_bets_plus_raises;
  msg.player_count = src->player_count;
  msg.round_over = src->round_over;
  msg.winner_declared = src->winner_declared;
  msg.n_rounds = src->n_rounds;

  // player
  Player *player_msgs[MAX_PLAYERS];
  struct player_message_builder_t builders[MAX_PLAYERS];

  for (int i = 0; i < MAX_PLAYERS; ++i) {
    fill_player_message(&builders[i], &src->player[i]);
    player_msgs[i] = &builders[i].msg;
  }

  msg.n_player = MAX_PLAYERS;
  msg.player = player_msgs;

  // Serialize to buffer
  *size_out = game_state__get_packed_size(&msg);
  uint8_t *buffer = malloc(*size_out);
  if (!buffer) {
    *size_out = 0;
    return NULL;
  }

  game_state__pack(&msg, buffer);
  return buffer;
}

game_state_t deserialize_game_state(const uint8_t *data, size_t size) {
  game_state_t result = {0};

  GameState *msg = game_state__unpack(NULL, size, data);
  if (!msg) {
    fprintf(stderr, "Failed to unpack GameState message\n");
    return result;
  }

  result.pot = msg->pot;
  result.dealer_id = msg->dealer_id;
  result.turn_id = msg->turn_id;
  result.at_menu = msg->at_menu;
  result.total_bets_plus_raises = msg->total_bets_plus_raises;
  result.player_count = msg->player_count;
  result.round_over = msg->round_over;
  result.winner_declared = msg->winner_declared;
  result.n_rounds = msg->n_rounds;

  size_t n = msg->n_player < MAX_PLAYERS ? msg->n_player : MAX_PLAYERS;
  for (size_t i = 0; i < n; ++i) {
    Player *pmsg = msg->player[i];
    if (!pmsg)
      continue;

    fill_player_from_message(&result.player[i], pmsg);
  }

  game_state__free_unpacked(msg, NULL);
  return result;
}

uint8_t *serialize_player(const struct player_t *src, size_t *size_out) {
  struct player_message_builder_t builder;
  fill_player_message(&builder, src);

  // Serialize to buffer
  *size_out = player__get_packed_size(&builder.msg);
  uint8_t *buffer = malloc(*size_out);
  if (!buffer) {
    *size_out = 0;
    return NULL;
  }

  player__pack(&builder.msg, buffer);
  return buffer;
}

struct player_t deserialize_player(const uint8_t *data, size_t size) {
  struct player_t result = {0};

  Player *msg = player__unpack(NULL, size, data);
  if (!msg) {
    fprintf(stderr, "Failed to unpack Player message\n");
    return result;
  }

  fill_player_from_message(&result, msg);

  player__free_unpacked(msg, NULL);
  return result;
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

int recv_all_tcp(TCPsocket sock, void *data, int length) {
  uint8_t *buf = (uint8_t *)data;
  int total_received = 0;

  while (total_received < length) {
    int received = SDLNet_TCP_Recv(sock, buf + total_received, (int)(length - total_received));
    if (received <= 0) {
      fprintf(stderr, "SDLNet_TCP_Recv failed or connection closed: %s\n", SDLNet_GetError());
      return received;
    }
    total_received += received;
  }

  return total_received;
}

int recv_game_state(TCPsocket client_socket, SDLNet_SocketSet socket_set,
                    game_state_t *game_state) {
  // printf("[recv_game_state] Waiting for game state...\n");
  int result = SDLNet_CheckSockets(socket_set, 100);
  if (result == -1) {
    fputs(SDLNet_GetError(), stderr);
    return -1;
  }

  if (result == 0) {
    return 0;
  }

  if (!SDLNet_SocketReady(client_socket)) {
    printf("[recv_game_state] client_socket not ready\n");
    return -1;
  }

  uint32_t size_net = 0;
  if (recv_all_tcp(client_socket, &size_net, sizeof(size_net)) <= 0) {
    fprintf(stderr, "[recv_game_state] Disconnected while reading game state size\n");
    return -1;
  }

  uint32_t size = ntohl(size_net);
  if (size == 0 || size > 65536) {
    fprintf(stderr, "[recv_game_state] Invalid game state size: %u\n", size);
    return -1;
  }

  uint8_t *buffer = malloc(size);
  if (!buffer) {
    fprintf(stderr, "[recv_game_state] Memory allocation failed\n");
    return -1;
  }

  if (recv_all_tcp(client_socket, buffer, size) <= 0) {
    fprintf(stderr, "[recv_game_state] Disconnected while reading game state payload\n");
    free(buffer);
    return -1;
  }

  printf("[recv_game_state] Received %u bytes, deserializing...\n", size);
  *game_state = deserialize_game_state(buffer, size);
  free(buffer);
  return 0;
}
