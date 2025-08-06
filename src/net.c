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
#include "util.h"

static void fill_player_message(struct player_message_builder_t *builder, const Player_t *src) {
  player__init(&builder->msg);
  hand__init(&builder->hand);

  // Name
  builder->msg.nick = (char *)src->nick;

  // ID and Chips
  builder->msg.id = src->id;
  builder->msg.coins = src->coins;
  builder->msg.in = src->in;
  builder->msg.winner = src->winner;
  builder->msg.is_connected = src->is_connected;

  // Hand
  for (int i = 0; i < MAX_HAND_SIZE; ++i) {
    card__init(&builder->cards[i]);
    builder->cards[i].face_val = src->hand.card[i].face_val;
    builder->cards[i].suit = src->hand.card[i].suit;
    builder->card_ptrs[i] = &builder->cards[i];
  }

  builder->hand.n_card = MAX_HAND_SIZE;
  builder->hand.card = builder->card_ptrs;
  builder->msg.hand = &builder->hand;
}

static void fill_player_from_message(Player_t *dst, const Player *msg) {
  if (!msg)
    return;

  if (msg->nick)
    snprintf(dst->nick, sizeof(dst->nick), "%s", msg->nick);

  dst->id = msg->id;
  dst->coins = msg->coins;
  dst->in = msg->in;
  dst->winner = msg->winner;
  dst->is_connected = msg->is_connected;

  if (msg->hand) {
    size_t n = msg->hand->n_card < MAX_HAND_SIZE ? msg->hand->n_card : MAX_HAND_SIZE;
    for (size_t i = 0; i < n; ++i) {
      dst->hand.card[i].face_val = msg->hand->card[i]->face_val;
      dst->hand.card[i].suit = msg->hand->card[i]->suit;
    }
  }
}

uint8_t *serialize_game_state(const GameState_t *src, uint32_t *size_out) {
  GameState msg = GAME_STATE__INIT;

  // Pot
  msg.pot = src->pot;
  msg.dealer_id = src->dealer_id;
  msg.at_menu = src->at_menu;
  msg.raises_remaining = src->raises_remaining;
  msg.player_count = src->player_count;
  msg.winner_declared = src->winner_declared;
  msg.deuces_wild = src->deuces_wild;

  // static Card wild_msg;
  // card__init(&wild_msg);
  // wild_msg.face_val = src->wild.face_val;
  // wild_msg.suit = src->wild.suit;
  // msg.wild = &wild_msg;

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

GameState_t deserialize_game_state(const uint8_t *data, uint32_t size) {
  GameState_t result = {0};

  GameState *msg = game_state__unpack(NULL, size, data);
  if (!msg) {
    fprintf(stderr, "Failed to unpack GameState message\n");
    return result;
  }

  result.pot = msg->pot;
  result.dealer_id = msg->dealer_id;
  result.at_menu = msg->at_menu;
  result.raises_remaining = msg->raises_remaining;
  result.player_count = msg->player_count;
  result.winner_declared = msg->winner_declared;
  result.deuces_wild = msg->deuces_wild;

  // if (msg->wild) {
  // result.wild.face_val = msg->wild->face_val;
  // result.wild.suit = msg->wild->suit;
  //} else {
  // result.wild = DH_card_null;
  //}

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

uint8_t *serialize_game_settings(const GameSettings_t *src, size_t *size_out) {
  GameSettings msg = GAME_SETTINGS__INIT;

  msg.client_id = src->client_id;
  msg.action_timeout_ms = src->action_timeout_ms;
  msg.end_of_game_timeout_ms = src->end_of_game_timeout_ms;

  // Serialize to buffer
  *size_out = game_settings__get_packed_size(&msg);
  uint8_t *buffer = malloc(*size_out);
  if (!buffer) {
    *size_out = 0;
    return NULL;
  }

  game_settings__pack(&msg, buffer);
  return buffer;
}

GameSettings_t deserialize_game_settings(const uint8_t *data, size_t size) {
  GameSettings_t result = {0};

  GameSettings *msg = game_settings__unpack(NULL, size, data);
  if (!msg) {
    fprintf(stderr, "Failed to unpack GameSettings message\n");
    return result;
  }

  result.client_id = msg->client_id;
  result.action_timeout_ms = msg->action_timeout_ms;
  result.end_of_game_timeout_ms = msg->end_of_game_timeout_ms;

  game_settings__free_unpacked(msg, NULL);
  return result;
}

uint8_t *serialize_hand(const POKEVAL_Hand_7 hand, size_t *size_out) {
  Hand proto_hand = HAND__INIT;
  Card proto_cards[MAX_HAND_SIZE];
  Card *proto_card_ptrs[MAX_HAND_SIZE]; // Array of pointers

  proto_hand.n_card = MAX_HAND_SIZE;
  proto_hand.card = proto_card_ptrs;

  for (size_t i = 0; i < MAX_HAND_SIZE; i++) {
    card__init(&proto_cards[i]);
    proto_cards[i].face_val = hand.card[i].face_val;
    proto_cards[i].suit = hand.card[i].suit;
    proto_card_ptrs[i] = &proto_cards[i]; // Point to each Card
  }

  *size_out = hand__get_packed_size(&proto_hand);
  uint8_t *buffer = malloc(*size_out);
  if (!buffer)
    return NULL;

  hand__pack(&proto_hand, buffer);
  return buffer;
}

POKEVAL_Hand_7 deserialize_hand(const uint8_t *data, size_t size) {
  POKEVAL_Hand_7 result = {0};
  Hand *proto_hand = hand__unpack(NULL, size, data);
  if (!proto_hand)
    return result;

  for (size_t i = 0; i < proto_hand->n_card && i < MAX_HAND_SIZE; i++) {
    result.card[i].face_val = proto_hand->card[i]->face_val;
    result.card[i].suit = proto_hand->card[i]->suit;
  }

  hand__free_unpacked(proto_hand, NULL);
  return result;
}

uint8_t *serialize_player(const Player_t *src, size_t *size_out) {
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

Player_t deserialize_player(const uint8_t *data, size_t size) {
  Player_t result = {0};

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
    // printf("Total sent: %zd\n", total_sent);
  }

  // TODO: This should probably return total sent
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

// Eventually some, or most, of the data in the game state struct will be sent
// via opcodes, like what's done for the discard/draw request
ERecvStatus_t recv_game_state(SocketContext_t *socket_context, GameState_t *game_state,
                              ClientState_t *client_state, const int8_t id) {
  // printf("[recv_game_state] Waiting for game state...\n");
  int result = SDLNet_CheckSockets(socket_context->set, 0);
  // printf("[recv_game_state] CheckSockets returned: %d\n", result);
  if (result == -1) {
    fputs(SDLNet_GetError(), stderr);
    return RECV_ERROR;
  }

  if (result == 0) {
    // This output can be particularly useful for debugging tests
    // fputs("[recv_game_state] No activity on socket\n", stderr);
    return RECV_NOTHING;
  }

  TCPsocket sock = socket_context->sock;
  if (!SDLNet_SocketReady(sock)) {
    printf("[recv_game_state] sock not ready\n");
    return RECV_ERROR;
  }

  uint32_t size_net = 0;
  int r_size = recv_all_tcp(sock, &size_net, sizeof(size_net));
  if (r_size <= 0) {
    fprintf(stderr, "[recv_game_state] Disconnected while reading game state size %d\n", r_size);
    return RECV_ERROR;
  }

  uint32_t size = SDL_SwapBE32(size_net);
  if (size == 0 || size > 65536) {
    fprintf(stderr, "[recv_game_state] Invalid game state size: %u\n", size);
    return RECV_ERROR;
  }

  uint8_t *buffer = malloc(size);
  if (!buffer) {
    fprintf(stderr, "[recv_game_state] Memory allocation failed\n");
    return RECV_ERROR;
  }

  if (recv_all_tcp(sock, buffer, size) <= 0) {
    fprintf(stderr, "[recv_game_state] Disconnected while reading game state payload\n");
    free(buffer);
    return RECV_ERROR;
  }

  uint16_t opcode_be;
  memcpy(&opcode_be, buffer, sizeof(opcode_be));
  uint16_t opcode = SDL_SwapBE16(opcode_be);
  // fprintf(stderr, "opcode: %04X\n", opcode);
  switch (opcode) {
  case MSG_TURN_ID:
    client_state->turn_id = (int8_t)buffer[2];
    client_state->turn_switch = true;
    break;
  case MSG_BET_CHECK_FOLD:
    client_state->bet_check_fold = true;
    break;
  case MSG_CALL_RAISE_FOLD:
    client_state->call_raise_fold = true;
    break;
  case MSG_DRAW_PROMPT:
    if (size != 2) {
      fprintf(stderr, "[recv_game_state] Invalid size for MSG_DRAW_PROMPT: %u\n", size);
      break;
    }
    client_state->do_discard_draw = true;
    client_state->n_cards_selected = 0;
    // printf("[recv_game_state] Received %u bytes, server wants discards...\n", size);
    break;

  case MSG_PING_REQUEST: {
    PingRequest *req = ping_request__unpack(NULL, size - 2, buffer + 2);
    if (!req) {
      fprintf(stderr, "[PING] Failed to unpack PingRequest\n");
      break;
    }

    // Prepare PingResponse with same timestamp
    PingResponse resp = PING_RESPONSE__INIT;
    resp.timestamp = req->timestamp;

    size_t len = ping_response__get_packed_size(&resp);
    uint8_t *buf = malloc(len);
    if (!buf) {
      ping_request__free_unpacked(req, NULL);
      break;
    }

    ping_response__pack(&resp, buf);

    // Send back response to server
    if (send_message(sock, MSG_PING_RESPONSE, buf, len) < 0) {
      fprintf(stderr, "[PING] Failed to send PingResponse\n");
    }

    free(buf);
    ping_request__free_unpacked(req, NULL);
  } break;

  case MSG_PING_BROADCAST: {
    PingBroadcast *pb = ping_broadcast__unpack(NULL, size - 2, buffer + 2);
    if (!pb) {
      fprintf(stderr, "[PING] Failed to unpack PingBroadcast\n");
      break;
    }

    // Store ping times by player ID
    for (size_t i = 0; i < pb->n_entries; i++) {
      int player_id = pb->entries[i]->player_id;
      if (player_id >= 0 && player_id < MAX_CLIENTS) {
        client_state->ping_times[player_id] = pb->entries[i]->ping_ms;
        verbose_printf("Player %d ping: %u ms\n", player_id, pb->entries[i]->ping_ms);
      }
    }

    ping_broadcast__free_unpacked(pb, NULL);
  } break;

  case MSG_WILD_REPLACEMENT:
    if (size != 2) {
      fprintf(stderr, "[recv_game_state] Invalid size for MSG_WILD_REPLACEMENTS: %u\n", size);
      break;
    }
    client_state->do_exchange_wilds = true;
    // client_state->n_cards_selected = 0;
    // printf("[recv_game_state] Received %u bytes, server wants wilds...\n", size);
    break;

  case MSG_STATUS_MESSAGE: {
    size_t msg_len = size - 2;
    snprintf(client_state->server_status_str, sizeof(client_state->server_status_str), "%.*s",
             (int)msg_len, (char *)&buffer[2]);
    // fprintf(stderr, "[Status Message] %s\n", client_state->server_status_str);
    if (strstr(client_state->server_status_str, "bet") ||
        strstr(client_state->server_status_str, "call") ||
        strstr(client_state->server_status_str, "raise"))
      client_state->play_coin_sound = true;
  } break;
  case MSG_NEW_HAND: {
    if (size < 3) {
      fputs("Invalid MSG_NEW_HAND payload (too short)\n", stderr);
      break;
    }

    uint8_t hand_size = buffer[2];
    uint8_t expected_size = 3 + hand_size * 8;
    if (hand_size == 0 || hand_size > MAX_HAND_SIZE || size != expected_size) {
      fprintf(stderr, "Invalid hand size or message length: %u\n", hand_size);
      break;
    }

    for (uint8_t i = 0; i < hand_size; ++i) {
      int8_t fv = buffer[3 + i * 2];
      int8_t s = buffer[3 + i * 2 + 1];
      game_state->player[id].hand.card[i].face_val = fv;
      game_state->player[id].hand.card[i].suit = s;
    }

    // printf("[recv_game_state] Received new hand with %u cards\n", hand_size);
    break;
  } break;
  default:
    // printf("[recv_game_state] Received %u bytes, deserializing...\n", size);
    *game_state = deserialize_game_state(buffer, size);
  }

  free(buffer);
  return RECV_SUCCESS;
}

ERecvStatus_t recv_game_settings(TCPsocket client_socket, SDLNet_SocketSet socket_set,
                                 GameSettings_t *game_settings) {
  int result = SDLNet_CheckSockets(socket_set, 100);
  if (result == -1) {
    fputs(SDLNet_GetError(), stderr);
    return RECV_ERROR;
  }

  if (result == 0) {
    // This output can be particularly useful for debugging tests
    // fputs("[recv_game_state] No activity on socket\n", stderr);
    return RECV_NOTHING;
  }

  if (!SDLNet_SocketReady(client_socket)) {
    printf("[recv_game_settings] client_socket not ready\n");
    return RECV_ERROR;
  }

  uint32_t size_net = 0;
  int r_size = recv_all_tcp(client_socket, &size_net, sizeof(size_net));
  if (r_size <= 0) {
    fprintf(stderr, "[recv_game_settings] Disconnected while reading game state size %d\n", r_size);
    return RECV_ERROR;
  }

  uint32_t size = SDL_SwapBE32(size_net);
  if (size == 0 || size > 65536) {
    fprintf(stderr, "[recv_game_settings] Invalid game settings size: %u\n", size);
    return RECV_ERROR;
  }

  uint8_t *buffer = malloc(size);
  if (!buffer) {
    fprintf(stderr, "[recv_game_settings] Memory allocation failed\n");
    return RECV_ERROR;
  }

  if (recv_all_tcp(client_socket, buffer, size) <= 0) {
    fprintf(stderr, "[recv_game_settings] Disconnected while reading game state payload\n");
    free(buffer);
    return RECV_ERROR;
  }

  verbose_printf("[recv_game_settings] Received %u bytes, deserializing...\n", size);
  *game_settings = deserialize_game_settings(buffer, size);

  free(buffer);
  return RECV_SUCCESS;
}

void socket_cleanup(SocketContext_t *socket_context) {
  if (SDLNet_TCP_DelSocket(socket_context->set, socket_context->sock) == -1)
    fputs(SDLNet_GetError(), stderr);
  SDLNet_FreeSocketSet(socket_context->set);
  SDLNet_TCP_Close(socket_context->sock);
}

int send_message(TCPsocket sock, uint16_t opcode, const uint8_t *payload, size_t payload_len) {
  uint32_t total_size = SDL_SwapBE32(2 + payload_len); // opcode + payload
  uint16_t opcode_be = SDL_SwapBE16(opcode);

  uint8_t header[6];
  memcpy(header, &total_size, 4);
  memcpy(header + 4, &opcode_be, 2);

  if (send_all_tcp(sock, header, sizeof(header)) < 0)
    return -1;
  if (send_all_tcp(sock, payload, payload_len) < 0)
    return -1;
  return 0;
}
