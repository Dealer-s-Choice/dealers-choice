/*
 net.c
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

#include "net.h"
#include "game.h"
#include "util.h"
#include <sodium.h>

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
  builder->msg.is_admin = src->is_admin;

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
  else
    dst->nick[0] = '\0';

  dst->id = (int8_t)msg->id;
  dst->coins = msg->coins;
  dst->in = msg->in;
  dst->winner = msg->winner;
  dst->is_connected = msg->is_connected;
  dst->is_admin = msg->is_admin;

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
  msg.round_opener_id = src->round_opener_id;
  msg.at_menu = src->at_menu;
  msg.raises_remaining = src->raises_remaining;
  msg.prev_bet_amount = src->prev_bet_amount;
  msg.player_count = src->player_count;
  msg.winner_declared = src->winner_declared;

  Player *player_msgs[MAX_PLAYERS];
  struct player_message_builder_t builders[MAX_PLAYERS];

  for (int i = 0; i < MAX_PLAYERS; ++i) {
    fill_player_message(&builders[i], &src->player[i]);
    player_msgs[i] = &builders[i].msg;
  }

  msg.n_player = MAX_PLAYERS;
  msg.player = player_msgs;

  // Serialize to buffer
  *size_out = (uint32_t)game_state__get_packed_size(&msg);
  uint8_t *buffer = malloc(*size_out);
  if (!buffer) {
    *size_out = 0;
    return NULL;
  }

  game_state__pack(&msg, buffer);
  return buffer;
}

bool deserialize_game_state(const uint8_t *data, uint32_t size, GameState_t *out) {
  GameState *msg = game_state__unpack(NULL, size, data);
  if (!msg) {
    fprintf(stderr, "Failed to unpack GameState message\n");
    return false;
  }

  out->pot = msg->pot;
  out->dealer_id = (int8_t)msg->dealer_id;
  out->round_opener_id = (int8_t)msg->round_opener_id;
  out->at_menu = msg->at_menu;
  out->raises_remaining = msg->raises_remaining;
  out->prev_bet_amount = msg->prev_bet_amount;
  out->player_count = (uint8_t)msg->player_count;
  out->winner_declared = msg->winner_declared;

  size_t n = msg->n_player < MAX_PLAYERS ? msg->n_player : MAX_PLAYERS;
  for (size_t i = 0; i < n; ++i) {
    Player *pmsg = msg->player[i];
    if (!pmsg)
      continue;

    fill_player_from_message(&out->player[i], pmsg);
  }

  game_state__free_unpacked(msg, NULL);
  return true;
}

uint8_t *serialize_game_settings(const GameSettings_t *src, size_t *size_out) {
  GameSettings msg = GAME_SETTINGS__INIT;

  msg.client_id = src->client_id;
  msg.action_timeout_ms = src->action_timeout_ms;
  msg.end_of_game_timeout_ms = src->end_of_game_timeout_ms;
  msg.n_bet_amounts = src->bet_amount_count;
  msg.bet_amounts = (uint32_t *)src->bet_amounts;

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

  result.client_id = (int8_t)msg->client_id;
  result.action_timeout_ms = msg->action_timeout_ms;
  result.end_of_game_timeout_ms = msg->end_of_game_timeout_ms;
  result.bet_amount_count = (uint8_t)msg->n_bet_amounts;
  for (size_t i = 0; i < msg->n_bet_amounts && i < MAX_BET_AMOUNTS; i++)
    result.bet_amounts[i] = msg->bet_amounts[i];

  game_settings__free_unpacked(msg, NULL);
  return result;
}

uint8_t *serialize_hand(const POKEVAL_Hand_9 hand, size_t *size_out) {
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

POKEVAL_Hand_9 deserialize_hand(const uint8_t *data, size_t size) {
  POKEVAL_Hand_9 result = {0};
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

int send_all_tcp(tcpme_socket_t sock, const void *data, size_t length) {
  const uint8_t *buf = (const uint8_t *)data;
  size_t total_sent = 0;

  while (total_sent < length) {
    int sent = tcpme_send(sock, buf + total_sent, (int)(length - total_sent));
    if (sent < 0) {
      fprintf(stderr, "tcpme_send failed: %s\n", tcpme_get_error());
      return -1;
    }
    if (sent == 0) {
      fprintf(stderr, "tcpme_send: connection closed\n");
      return -1;
    }
    total_sent += sent;
  }

  return 0;
}

int recv_all_tcp(tcpme_socket_t sock, void *buf, size_t len) {
  uint8_t *p = buf;
  size_t total = 0;

  while (total < len) {
    int r = tcpme_recv(sock, p + total, (int)(len - total));

    if (r <= 0)
      return -1;

    total += (size_t)r;
  }
  return (int)total;
}

// Eventually some, or most, of the data in the game state struct will be sent
// via opcodes, like what's done for the discard/draw request
ERecvStatus_t recv_game_state(SocketContext_t *socket_context, GameState_t *game_state,
                              ClientState_t *client_state, const int8_t id) {
  /* Loop so that server-initiated ping messages are handled transparently:
   * they are processed and the next message is read without returning to the
   * caller, so callers never need to account for pings in their receive count. */
  for (;;) {
    int result = tcpme_check_sockets(socket_context->set, 0);
    if (result == -1) {
      fputs(tcpme_get_error(), stderr);
      return RECV_ERROR;
    }
    if (result == 0)
      return RECV_NOTHING;

    tcpme_socket_t sock = socket_context->sock;
    if (!tcpme_socket_ready(socket_context->set, sock)) {
      fprintf(stderr, "[recv_game_state] sock not ready\n");
      return RECV_ERROR;
    }

    uint32_t size_net = 0;
    int r_size = recv_all_tcp(sock, &size_net, sizeof(size_net));
    if (r_size != (int)sizeof(size_net)) {
      fprintf(stderr, "[recv_game_state] Disconnected while reading game state size (%d).\n",
              r_size);
      return RECV_ERROR;
    }

    uint32_t size = tcpme_get_be32((const uint8_t *)&size_net);
    if (size == 0 || size > 65536) {
      fprintf(stderr, "[recv_game_state] Invalid game state size: %u\n", size);
      return RECV_ERROR;
    }

    uint8_t *buffer = malloc(size);
    if (!buffer) {
      fprintf(stderr, "[recv_game_state] Memory allocation failed\n");
      return RECV_ERROR;
    }

    int r_payload = recv_all_tcp(sock, buffer, size);
    if (r_payload != (int)size) {
      fprintf(stderr,
              "[recv_game_state] Disconnected while reading game state payload (got %d, expected "
              "%u). tcpme_get_error(): %s\n",
              r_payload, size, tcpme_get_error());
      free(buffer);
      return RECV_ERROR;
    }

    uint16_t opcode = tcpme_get_be16(buffer);
    // fprintf(stderr, "opcode: %04X\n", opcode);
    bool transparent = false;
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
    case MSG_CALL_COMPLETE_FOLD:
      client_state->call_complete_fold = true;
      break;
    case MSG_COMPLETE_CHECK_FOLD:
      client_state->complete_check_fold = true;
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
      uint8_t buf[16]; // ample for one uint32 varint field
      ping_response__pack(&resp, buf);

      // Send back response to server
      if (send_message(sock, MSG_PING_RESPONSE, buf, len) < 0) {
        fprintf(stderr, "[PING] Failed to send PingResponse\n");
      }

      ping_request__free_unpacked(req, NULL);
      transparent = true;
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
          // verbose_printf("[PING] Player %d: %u ms\n", player_id, pb->entries[i]->ping_ms);
        }
      }

      ping_broadcast__free_unpacked(pb, NULL);
      transparent = true;
    } break;

    case MSG_STATUS_MESSAGE: {
      size_t msg_len = size - 2;
      snprintf(client_state->server_status_str, sizeof(client_state->server_status_str), "%.*s",
               (int)msg_len, (char *)&buffer[2]);
      // fprintf(stderr, "[Status Message] %s\n", client_state->server_status_str);
    } break;

    case MSG_ACTION_ANNOUNCE: {
      ActionAnnounce *aa = action_announce__unpack(NULL, size - OPCODE_SIZE, buffer + OPCODE_SIZE);
      if (!aa) {
        fputs("Failed to unpack ActionAnnounce protobuf\n", stderr);
        break;
      }
      client_state->action_announce_pending = true;
      client_state->action_announce_player = (int8_t)aa->player_id;
      client_state->action_announce_verb = aa->verb;
      client_state->action_announce_amount = aa->amount;
      if (aa->verb == ANNOUNCE_BET || aa->verb == ANNOUNCE_CALLED || aa->verb == ANNOUNCE_RAISED ||
          aa->verb == ANNOUNCE_COMPLETED)
        client_state->play_coin_sound = true;
      action_announce__free_unpacked(aa, NULL);
    } break;

    case MSG_NEW_HAND: {
      if (size < OPCODE_SIZE + 1) { // minimal valid payload: at least opcode + 1 byte of data
        fputs("Invalid MSG_NEW_HAND payload (too short)\n", stderr);
        break;
      }

      // Unpack protobuf starting after the opcode
      Hand *pb_hand = hand__unpack(NULL, size - OPCODE_SIZE, buffer + OPCODE_SIZE);
      if (!pb_hand) {
        fputs("Failed to unpack Hand protobuf\n", stderr);
        break;
      }

      if (pb_hand->n_card == 0 || pb_hand->n_card > MAX_HAND_SIZE) {
        fprintf(stderr, "Invalid hand size: %zu\n", pb_hand->n_card);
        hand__free_unpacked(pb_hand, NULL);
        break;
      }

      for (uint32_t i = 0; i < pb_hand->n_card; ++i) {
        game_state->player[id].hand.card[i].face_val = pb_hand->card[i]->face_val;
        game_state->player[id].hand.card[i].suit = pb_hand->card[i]->suit;
      }

      hand__free_unpacked(pb_hand, NULL);
      break;
    }
    case MSG_GAME_SELECT: {
      GameSelectPayload_t payload = {0};
      get_game_select_payload(buffer, size, id, &payload);
      client_state->game_type = payload.game_type;
      client_state->deuces_wild = payload.deuces_wild;
      client_state->game_choice = find_game_choice_by_type(client_state->game_type);
      break;
    }

    default:
      if (!deserialize_game_state(buffer, size, game_state))
        fprintf(stderr,
                "[recv_game_state] unrecognized opcode 0x%04X (size=%u) could not be parsed as "
                "GameState\n",
                opcode, size);
    }

    free(buffer);
    if (!transparent)
      return RECV_SUCCESS;
  } /* for (;;) */
}

ERecvStatus_t recv_game_settings(tcpme_socket_t client_socket, tcpme_set_t *socket_set,
                                 GameSettings_t *game_settings) {
  int result = tcpme_check_sockets(socket_set, 100);
  if (result == -1) {
    fputs(tcpme_get_error(), stderr);
    return RECV_ERROR;
  }

  if (result == 0) {
    // This output can be particularly useful for debugging tests
    // fputs("[recv_game_state] No activity on socket\n", stderr);
    return RECV_NOTHING;
  }

  if (!tcpme_socket_ready(socket_set, client_socket)) {
    printf("[recv_game_settings] client_socket not ready\n");
    return RECV_ERROR;
  }

  uint32_t size_net = 0;
  int r_size = recv_all_tcp(client_socket, &size_net, sizeof(size_net));
  if (r_size <= 0) {
    fprintf(stderr, "[recv_game_settings] Disconnected while reading game state size %d\n", r_size);
    return RECV_ERROR;
  }

  uint32_t size = tcpme_get_be32((const uint8_t *)&size_net);
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
  if (tcpme_del_socket(socket_context->set, socket_context->sock) == -1)
    fputs(tcpme_get_error(), stderr);
  tcpme_free_set(socket_context->set);
  tcpme_close(socket_context->sock);
}

int send_message(tcpme_socket_t sock, uint16_t opcode, const uint8_t *payload, size_t payload_len) {
  if (payload_len > UINT32_MAX - 2)
    return -1;
  uint32_t total_size;
  tcpme_put_be32((uint8_t *)&total_size, (uint32_t)(2 + payload_len));
  uint16_t opcode_be;
  tcpme_put_be16((uint8_t *)&opcode_be, opcode);

  if (send_all_tcp(sock, &total_size, sizeof(total_size)) < 0)
    return -1;

  if (send_all_tcp(sock, &opcode_be, sizeof(opcode_be)) < 0)
    return -1;

  if (payload_len > 0) {
    if (send_all_tcp(sock, payload, payload_len) < 0)
      return -1;
  }

  return 0;
}

int send_protocol_header(tcpme_socket_t sock, uint8_t flags) {
  verbose_puts("Exchanging protocol information...");
  GameProtocolHeader_t hdr = {0};
  snprintf(hdr.magic, sizeof(hdr.magic), "%s", GAME_PROTOCOL_MAGIC);
  tcpme_put_be16((uint8_t *)&hdr.version, GAME_PROTOCOL_VERSION);
  hdr.flags = flags;

  if (send_all_tcp(sock, &hdr, sizeof(hdr)) != 0)
    return -1;

  uint8_t response;
  if (recv_all_tcp(sock, &response, sizeof(response)) <= 0) {
    fprintf(stderr, "Protocol version mismatch or server closed connection\n");
    return -1;
  }
  if (response != 0) {
    fprintf(stderr,
            "Server rejected connection: protocol version mismatch "
            "(client version: %d)\n",
            GAME_PROTOCOL_VERSION);
    return -1;
  }

  return 0;
}

int authenticate_with_server(tcpme_socket_t sock, const char *password) {
  unsigned char nonce[NONCE_SIZE];
  unsigned char hash[HASH_SIZE];

  if (recv_all_tcp(sock, nonce, NONCE_SIZE) < 0) {
    fprintf(stderr, "Failed to receive nonce\n");
    return -1;
  }

  crypto_hash_sha256_state state;
  crypto_hash_sha256_init(&state);
  crypto_hash_sha256_update(&state, (const unsigned char *)password, strlen(password));
  crypto_hash_sha256_update(&state, nonce, NONCE_SIZE);
  crypto_hash_sha256_final(&state, hash);

  if (send_all_tcp(sock, hash, HASH_SIZE) != 0) {
    fprintf(stderr, "Failed to send authentication response\n");
    return -1;
  }

  return 0;
}

bool bot_connect(const char *host_str, uint16_t port, const char *nick, const char *password,
                 SocketContext_t *out) {
  tcpme_socket_t sock = tcpme_connect(host_str, port);
  if (!tcpme_socket_valid(sock)) {
    fprintf(stderr, "Failed to connect: %s\n", tcpme_get_error());
    return false;
  }
  tcpme_set_timeout(sock, SOCKET_IO_TIMEOUT_MS);

  tcpme_set_t *set = tcpme_alloc_set(1);
  if (!set) {
    tcpme_close(sock);
    return false;
  }
  tcpme_add_socket(set, sock);

  if (send_protocol_header(sock, PROTO_FLAG_BOT) != 0)
    goto fail;

  if (authenticate_with_server(sock, password ? password : "") < 0) {
    fprintf(stderr, "Authentication failed\n");
    goto fail;
  }

  {
    uint16_t len = (uint16_t)strlen(nick);
    uint16_t net_len;
    tcpme_put_be16((uint8_t *)&net_len, len);
    if (send_all_tcp(sock, &net_len, sizeof(net_len)) != 0 || send_all_tcp(sock, nick, len) != 0) {
      fprintf(stderr, "Failed to send nick\n");
      goto fail;
    }
  }

  out->sock = sock;
  out->set = set;
  return true;

fail:
  tcpme_free_set(set);
  tcpme_close(sock);
  return false;
}

int send_player_action(ClientState_t *client_state, tcpme_socket_t sock, uint8_t action,
                       uint32_t amount) {
  uint8_t payload[5];
  payload[0] = action;
  payload[1] = (amount >> 24) & 0xFF;
  payload[2] = (amount >> 16) & 0xFF;
  payload[3] = (amount >> 8) & 0xFF;
  payload[4] = (amount) & 0xFF;

  static const char *action_names[] = {NULL, "check", "call", "bet", "raise", "fold"};
  if (amount > 0)
    verbose_printf("%s %u\n", action_names[action], amount);
  else
    verbose_puts(action_names[action]);

  client_state->bet_check_fold = false;
  client_state->call_raise_fold = false;

  int rc = send_message(sock, MSG_PLAYER_ACTION, payload, sizeof(payload));
  if (rc != 0)
    fputs("Failed to send action\n", stderr);
  return rc;
}

int send_discards_request_new_cards(tcpme_socket_t sock, const uint8_t *discard_indices,
                                    uint8_t count) {
  if (count > 4)
    return -1;

  uint8_t payload[5] = {0};
  payload[0] = count;
  for (int i = 0; i < 4; ++i)
    payload[1 + i] = (i < count) ? discard_indices[i] : 0xFF;

  int rc = send_message(sock, MSG_DRAW_REQUEST, payload, sizeof(payload));
  if (rc != 0)
    fputs("Failed to send discards\n", stderr);
  return rc;
}
