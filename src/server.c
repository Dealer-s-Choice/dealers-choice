/*
 server.c
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

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "dc_config.h"
#include "game.h"
#include "server.h"
#include "util.h"

#define MAX_DISCARDS 4
#define MAX_WILDS 4

#define SEND_RETRY_COUNT 3
#define SEND_RETRY_DELAY_MS 500
#define PING_THRESHOLD 1000

#define handle_round() handle_round_real(args)

typedef struct {
  uint8_t n_winners;
  int id[MAX_PLAYERS];
} RoundResults;

typedef struct {
  uint8_t discard_count;
  uint8_t discard_indices[MAX_DISCARDS];
} DrawRequestMsg_t;

static void remove_disconnected_player(ArgsBroadcastGameState_t *args, const int8_t id);

static bool handle_disconnections(ArgsBroadcastGameState_t *args);

static uint8_t count_active_clients(const bool *slot_taken);

typedef enum { LOOP_BREAK, LOOP_CONTINUE, LOOP_OK, LOOP_ERROR } ELoop_t;
static ELoop_t register_new_client(ArgsBroadcastGameState_t *args);

static void print_ipaddress(const IPaddress *ip) {
  char ipaddr[INET6_ADDRSTRLEN];
  Uint32 host = SDL_SwapBE32(ip->host);

  if (host == 0) {
    snprintf(ipaddr, sizeof(ipaddr), "0.0.0.0");
  } else {
    snprintf(ipaddr, sizeof(ipaddr), "%u.%u.%u.%u", (host >> 24) & 0xFF, (host >> 16) & 0xFF,
             (host >> 8) & 0xFF, host & 0xFF);
  }

  printf("%s:%u\n", ipaddr, SDL_SwapBE16(ip->port));
}

ServerConfig_t init_game_state(GameState_t *game_state, Path_t *path, const CliArgs_t *cli_args) {
  ServerConfig_t config = get_server_config(path, cli_args);

  const int max_starting_coins = 99999999;
  if (config.starting_coins > max_starting_coins) {
    fprintf(stderr, "Error: starting_coins too high (max %d)\n", max_starting_coins);
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < MAX_PLAYERS; i++) {
    game_state->player[i] = (Player_t){
        .id = i,
        .is_connected = false,
        .coins = config.starting_coins,
        .in = false,
        .winner = false,
    };
    snprintf(game_state->player[i].nick, sizeof game_state->player[i].nick, "Player %d", i);
  }

  game_state->dealer_id = 0;
  game_state->at_menu = true;
  game_state->player_count = 0;
  game_state->raises_remaining = 0;
  game_state->prev_bet_amount = 0;
  game_state->winner_declared = false;
  game_state->deuces_wild = false;
  game_state->player_exchanging = false;
  return config;
}

GameSettings_t init_game_settings(const ServerConfig_t *config, const CliArgs_t *cli_args) {
  const unsigned int max_bet_amounts = 9999999;
  if (config->bet_minimum > max_bet_amounts || config->bet_median > max_bet_amounts ||
      config->bet_maximum > max_bet_amounts) {
    fprintf(stderr, "Bet amounts must be <= %d.\n", max_bet_amounts);
    exit(EXIT_FAILURE);
  }

  GameSettings_t game_settings = {
      .action_timeout_ms = config->action_timeout_ms,
      .wild_exchange_timeout_ms = config->wild_exchange_timeout_ms,
      .end_of_game_timeout_ms = (cli_args->test_mode) ? 500 : config->end_of_game_timeout_ms,
      .bet_minimum = config->bet_minimum,
      .bet_median = config->bet_median,
      .bet_maximum = config->bet_maximum,
  };
  return game_settings;
}

// In the future, hands will be sent using functions like this, rather than how it's
// presently done in broadcast_game_state()
static int send_new_hand(TCPsocket sock, const POKEVAL_Hand_7 *hand, uint8_t hand_size) {
  if (hand_size == 0 || hand_size > MAX_HAND_SIZE)
    return -1;

  Hand pb_hand = HAND__INIT;
  Card *cards[MAX_HAND_SIZE];

  for (uint8_t i = 0; i < hand_size; ++i) {
    cards[i] = calloc_wrap(sizeof(Card), 1);
    *cards[i] = (Card)CARD__INIT;
    cards[i]->face_val = hand->card[i].face_val;
    cards[i]->suit = hand->card[i].suit;
  }

  pb_hand.n_card = hand_size;
  pb_hand.card = cards;

  size_t packed_size = hand__get_packed_size(&pb_hand);
  uint32_t payload_size = OPCODE_SIZE + packed_size;
  uint32_t total_size = SDL_SwapBE32(payload_size);

  uint8_t *buffer = malloc(LENGTH_PREFIX_SIZE + payload_size);
  memcpy(buffer, &total_size, LENGTH_PREFIX_SIZE);
  buffer[LENGTH_PREFIX_SIZE] = (MSG_NEW_HAND >> 8) & 0xFF;
  buffer[LENGTH_PREFIX_SIZE + 1] = MSG_NEW_HAND & 0xFF;

  hand__pack(&pb_hand, buffer + LENGTH_PREFIX_SIZE + OPCODE_SIZE);

  int result = send_all_tcp(sock, buffer, LENGTH_PREFIX_SIZE + payload_size);

  free(buffer);
  for (uint8_t i = 0; i < hand_size; ++i)
    free(cards[i]);

  return result;
}

RealHand_t deal_cards_to_players(GameState_t *game_state, DH_Deck *deck, const uint8_t game_type) {
  RealHand_t real_hand = {0};
  Player_t *players_array = game_state->player;
  Player_t *turn = get_next_player(players_array, game_state->dealer_id);
  Player_t *starting_turn = turn;

  const GameChoice_t *choice = find_game_choice_by_type(game_type);

  size_t i = 0;
  do {
    if (choice->n_stud_new_cards == 0) {
      for (i = 0; i < MAX_HAND_SIZE; ++i) {
        if (i >= choice->hand_size) {
          real_hand.player[turn->id].card[i] = DH_card_null;
          turn->hand.card[i] = DH_card_null;
          continue;
        }
        turn->hand.card[i] = DH_card_back;
        real_hand.player[turn->id].card[i] = DH_deal_top_card(deck);
      }
    } else {
      int cards_dealt = 0;
      POKEVAL_Hand_7 *hand = &turn->hand;

      for (i = 0; i < 2; i++) {
        // First card face down
        hand->card[cards_dealt] = DH_card_back;
        real_hand.player[turn->id].card[cards_dealt] = DH_deal_top_card(deck);
        cards_dealt++;
        if (game_type == game_choices[FIVE_CARD_STUD].game_type)
          break;
      }

      // Next card face up
      hand->card[cards_dealt] = DH_deal_top_card(deck);
      real_hand.player[turn->id].card[cards_dealt] = hand->card[cards_dealt];
      cards_dealt++;

      for (i = cards_dealt; i < MAX_HAND_SIZE; i++) {
        hand->card[i] = DH_card_null;
        real_hand.player[turn->id].card[i] = hand->card[i];
      }
    }
    turn = get_next_player(players_array, turn->id);
  } while (turn && turn != starting_turn);

  return real_hand;
}

int send_with_retries(TCPsocket sock, const void *data, size_t size) {
  for (int attempt = 0; attempt < SEND_RETRY_COUNT; ++attempt) {
    if (send_all_tcp(sock, data, size) == 0)
      return 0;
    SDL_Delay(SEND_RETRY_DELAY_MS);
  }
  return -1;
}

static void broadcast_game_state(ArgsBroadcastGameState_t *args) {
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (!args->clients[i]) {
      // fprintf(stderr, "skipping %d\n", i);
      continue;
    }

    POKEVAL_Hand_7 hand_tmp = {0};
    if (args->game_state->winner_declared && args->game_state->player_count != 1) {
      memcpy(&args->game_state->player[i].hand, &args->real_hand->player[i],
             sizeof(POKEVAL_Hand_7));

    } else {
      memcpy(&hand_tmp, &args->game_state->player[i].hand, sizeof(POKEVAL_Hand_7));
      memcpy(&args->game_state->player[i].hand, &args->real_hand->player[i],
             sizeof(POKEVAL_Hand_7));
    }

    uint32_t size = 0;
    uint8_t *data = serialize_game_state(args->game_state, &size);
    if (!data)
      return;

    if (!args->game_state->winner_declared || args->game_state->player_count == 1)
      memcpy(&args->game_state->player[i].hand, &hand_tmp, sizeof(POKEVAL_Hand_7));

    uint32_t size_net = SDL_SwapBE32(size);

    // fprintf(stderr, "sending to %d\n", i);
    if (send_with_retries(args->clients[i], &size_net, sizeof(size_net)) == -1 ||
        send_with_retries(args->clients[i], data, size) == -1) {
      fprintf(stderr, "Failed to send game state to client %d after retries\n", i);
      handle_disconnections(args);
    }
    free(data);
  }
}

static void send_game_settings(ArgsBroadcastGameState_t *args, TCPsocket sock) {
  size_t size = 0;
  uint8_t *data = serialize_game_settings(args->game_settings, &size);
  if (!data)
    return;

  uint32_t size_net = SDL_SwapBE32(size);

  // fprintf(stderr, "sending to %d\n", i);
  if (send_with_retries(sock, &size_net, sizeof(size_net)) == -1 ||
      send_with_retries(sock, data, size) == -1) {
    fprintf(stderr, "Failed to send game settings to client after retries\n");
    handle_disconnections(args);
  }
  free(data);
}

int send_status_message(TCPsocket sock, const char *msg) {
  size_t msg_len = strlen(msg);
  if (msg_len > LEN_STATUS_STR)
    msg_len = LEN_STATUS_STR;

  uint32_t size = SDL_SwapBE32(2 + msg_len); // payload: 2-byte opcode + N-byte msg
  uint8_t buffer[4 + 2 + LEN_STATUS_STR];    // max: 4 bytes (size) + 2 (opcode) + 100 (msg)

  memcpy(buffer, &size, sizeof(size));

  uint16_t opcode_be = SDL_SwapBE16(MSG_STATUS_MESSAGE);
  memcpy(&buffer[4], &opcode_be, sizeof(opcode_be));

  memcpy(&buffer[6], msg, msg_len);

  // Send total (size prefix + payload)
  return send_all_tcp(sock, buffer, 6 + msg_len);
}

static void broadcast_status_message(const ArgsBroadcastGameState_t *args, const char *msg) {
  if (count_active_clients(args->slot_taken) == 0)
    return;
  int8_t pl_idx = args->turn_id;
  Player_t *recipient = &args->game_state->player[pl_idx];
  if (!recipient->is_connected)
    recipient = get_next_connected_client(args->game_state->player, pl_idx);
  Player_t *start = recipient;

  do {
    pl_idx = recipient->id;
    TCPsocket sock = args->clients[pl_idx];
    if (!sock)
      continue;

    if (send_status_message(sock, msg) < 0) {
      fprintf(stderr, "[broadcast_status_message] Failed to send to client %d\n", pl_idx);
    }

    recipient = get_next_connected_client(args->game_state->player, pl_idx);
  } while (recipient && recipient != start);
}

static int send_turn_id(TCPsocket sock, const int8_t turn_id) {
  uint8_t buffer[7];

  uint32_t size = SDL_SwapBE32(3); // payload = 2-byte opcode + 1-byte turn_id
  memcpy(buffer, &size, sizeof(size));

  uint16_t opcode_be = SDL_SwapBE16(MSG_TURN_ID);
  memcpy(&buffer[4], &opcode_be, sizeof(opcode_be));

  buffer[6] = (uint8_t)turn_id;

  return send_all_tcp(sock, buffer, sizeof(buffer));
}

void broadcast_turn_id(const ArgsBroadcastGameState_t *args) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!args->clients[i])
      continue;

    TCPsocket sock = args->clients[i];
    if (!sock)
      continue;

    if (send_turn_id(sock, args->turn_id) < 0) {
      fprintf(stderr, "[broadcast_turn_id] Failed to send to client %d\n", i);
    }
  }
}

static int recv_player_action(TCPsocket sock, PlayerActionMsg_t *out_action) {
  uint8_t buffer[7];

  int n_bytes;
  if ((n_bytes = recv_all_tcp(sock, buffer, sizeof(buffer))) <= 0) {
    fputs("Failed to receive player action\n", stderr);
    return n_bytes;
  }

  uint16_t opcode = (buffer[0] << 8) | buffer[1];
  if (opcode != MSG_PLAYER_ACTION) {
    fprintf(stderr, "[%s] Incorrect opcode\n", __func__);
    return -1;
  }

  out_action->action = buffer[2];
  out_action->amount = ((uint32_t)buffer[3] << 24) | ((uint32_t)buffer[4] << 16) |
                       ((uint32_t)buffer[5] << 8) | ((uint32_t)buffer[6]);

  verbose_printf("Received action %u with amount %" PRIu32 "\n", out_action->action,
                 out_action->amount);
  return n_bytes;
}

static int send_opcode(TCPsocket sock, const uint16_t opcode) {
  uint8_t buffer[6];

  uint32_t size = SDL_SwapBE32(2);
  memcpy(buffer, &size, sizeof(size));

  uint16_t opcode_be = SDL_SwapBE16(opcode);
  memcpy(&buffer[4], &opcode_be, sizeof(opcode_be));

  int sent = send_all_tcp(sock, buffer, sizeof(buffer));
  if (sent == 0)
    verbose_puts("Sent opcode");
  return sent;
}

static int send_ping_request(TCPsocket sock) {
  PingRequest req = PING_REQUEST__INIT;
  req.timestamp = SDL_GetTicks(); // current server tick

  size_t len = ping_request__get_packed_size(&req);
  uint8_t *buf = malloc(len);
  if (!buf)
    return -1;

  ping_request__pack(&req, buf);

  int result = send_message(sock, MSG_PING_REQUEST, buf, len);

  free(buf);
  return result;
}

static int broadcast_ping_times(ArgsBroadcastGameState_t *args, const uint32_t ping_times[]) {
  PingBroadcast pb = PING_BROADCAST__INIT;
  PingEntry entries[MAX_CLIENTS];
  PingEntry *entry_ptrs[MAX_CLIENTS];

  size_t count = 0;
  for (int j = 0; j < MAX_CLIENTS; j++) {
    if (!args->clients[j])
      continue;
    ping_entry__init(&entries[count]);
    entries[count].player_id = j;
    entries[count].ping_ms = ping_times[j];
    entry_ptrs[count] = &entries[count];
    count++;
  }

  pb.n_entries = count;
  pb.entries = entry_ptrs;

  size_t len = ping_broadcast__get_packed_size(&pb);
  uint8_t *buf = malloc(len);
  if (!buf)
    return -1;
  ping_broadcast__pack(&pb, buf);

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!args->clients[i])
      continue;
    int result = send_message(args->clients[i], MSG_PING_BROADCAST, buf, len);
    if (result < 0) {
      fprintf(stderr, "[PING] Failed to broadcast to client %d\n", i);
    }
  }
  free(buf);
  return 0;
}

static void server_handle_call(GameState_t *game_state, uint32_t *total_paid, const uint8_t turn_id,
                               uint32_t *total_bets_plus_raises) {
  uint32_t owed = *total_bets_plus_raises - *total_paid;
  game_state->player[turn_id].coins -= owed;
  *total_paid += owed;
  game_state->pot += owed;
}

static void server_handle_ante(GameState_t *game_state, const uint32_t amount) {
  Player_t *dealer = &game_state->player[game_state->dealer_id];
  Player_t *turn = dealer;
  do {
    if (game_state->player[turn->id].in) {
      game_state->player[turn->id].coins -= amount;
      game_state->pot += amount;
    }
    turn = get_next_player(game_state->player, turn->id);
  } while (turn && turn != dealer);
}

static void server_handle_bet(GameState_t *game_state, uint32_t *total_paid, const uint8_t turn_id,
                              const uint32_t amount, uint32_t *total_bets_plus_raises) {
  game_state->player[turn_id].coins -= amount;
  *total_paid += amount;
  *total_bets_plus_raises += amount;
  game_state->prev_bet_amount = amount;
  game_state->pot += amount;
}

// On Ubuntu 24.04 arm64: error: conflicting types for ‘raise’; so I've given
// this a more unique name now
static void server_handle_raise(GameState_t *game_state, uint32_t *total_paid,
                                const uint8_t turn_id, const uint32_t amount,
                                uint32_t *total_bets_plus_raises) {
  server_handle_call(game_state, total_paid, turn_id, total_bets_plus_raises);
  server_handle_bet(game_state, total_paid, turn_id, amount, total_bets_plus_raises);
  game_state->raises_remaining--;
}

static void handle_sort_hand(POKEVAL_Hand_7 *real_hand, const bool is_lowball) {
  POKEVAL_Hand_5 tmp_hand = POKEVAL_hand5_from_hand7(real_hand);
  if (!is_lowball)
    POKEVAL_sort_hand(&tmp_hand);
  else
    POKEVAL_sort_hand_lowball(&tmp_hand);
  memcpy(&real_hand->card[0], &tmp_hand.card[0], sizeof(tmp_hand.card));

  /* Step 4: ensure last two cards are NULL */
  // This will remove the last two cards from a 7-card stud hand, otherwise they show up
  // even if they are duplicates of cards among the best 5 cards in that player's hand
  real_hand->card[5] = DH_card_null;
  real_hand->card[6] = DH_card_null;
}

static ELoop_t handle_draw(ArgsBroadcastGameState_t *args, TCPsocket sock, const int id,
                           DH_Deck *deck) {
  verbose_puts("sending draw prompt");
  if (send_opcode(sock, MSG_DRAW_PROMPT) != 0) {
    fputs("Failed to send draw prompt\n", stderr);
    return LOOP_ERROR;
  }

  DrawRequestMsg_t req;
  uint8_t buffer[7] = {0};

  uint32_t wait_ms = args->game_settings->action_timeout_ms;
  uint32_t start = SDL_GetTicks();
  int n_bytes = 0;
  while (SDL_GetTicks() - start < wait_ms) {
    register_new_client(args);
    int num_ready = SDLNet_CheckSockets(args->socket_set, 0);
    if (num_ready == -1) {
      fprintf(stderr, "SDLNet_CheckSockets: %s\n", SDLNet_GetError());
      return LOOP_ERROR;
    }
    if (num_ready > 0) {
      if (SDLNet_SocketReady(sock)) {
        n_bytes = recv_all_tcp(sock, buffer, sizeof(buffer));
        if (n_bytes > 0)
          break;
        else {
          remove_disconnected_player(args, id);
          broadcast_game_state(args);
          return LOOP_BREAK;
        }
      } else {
        if (handle_disconnections(args))
          broadcast_game_state(args);
        if (args->game_state->player_count == 1)
          return LOOP_BREAK;
      }
    }
  }

  if (*buffer != 0) {
    uint16_t opcode = (buffer[0] << 8) | buffer[1];
    if (opcode != MSG_DRAW_REQUEST)
      return LOOP_ERROR;
  }

  uint8_t count = buffer[2];
  if (count > MAX_DISCARDS)
    return LOOP_ERROR;

  req.discard_count = count;
  memcpy(req.discard_indices, &buffer[3], MAX_DISCARDS); // copy all 4

  // printf("Player wants to discard %u cards: ", req.discard_count);
  for (int i = 0; i < req.discard_count; ++i) {
    // printf("%u ", req.discard_indices[i]);
    args->real_hand->player[id].card[req.discard_indices[i]] = DH_deal_top_card(deck);
    // puts("");
  }

  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof status_str, "%s drew %d", args->game_state->player[id].nick,
           req.discard_count);
  broadcast_status_message(args, status_str);

  if (req.discard_count > 0) {
    handle_sort_hand(&args->real_hand->player[id],
                     args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type);
    send_new_hand(sock, &args->real_hand->player[id], MAX_HAND_SIZE);
  }

  return LOOP_OK;
}

static ELoop_t handle_wild_cards(ArgsBroadcastGameState_t *args, TCPsocket sock, const int id) {
  verbose_puts("sending submit wild prompt");
  if (send_opcode(sock, MSG_WILD_REPLACEMENT) != 0) {
    fputs("Failed to send submit wild prompt\n", stderr);
    return LOOP_ERROR;
  }

  const uint32_t wait_ms = args->game_settings->wild_exchange_timeout_ms;
  const uint32_t start = SDL_GetTicks();

  POKEVAL_Hand_7 received_hand = {0};
  while (SDL_GetTicks() - start < wait_ms) {
    register_new_client(args);
    int num_ready = SDLNet_CheckSockets(args->socket_set, 0);
    if (num_ready == -1) {
      fprintf(stderr, "SDLNet_CheckSockets: %s\n", SDLNet_GetError());
      return LOOP_ERROR;
    }

    if (num_ready > 0) {
      if (SDLNet_SocketReady(sock)) {
        uint8_t buffer[512] = {0}; // pick a reasonable buffer size
        int n_bytes = SDLNet_TCP_Recv(sock, buffer, sizeof(buffer));
        if (n_bytes > 0) {
          received_hand = deserialize_hand(buffer, n_bytes);
          break;
        } else {
          remove_disconnected_player(args, id);
          broadcast_game_state(args);
          return LOOP_BREAK;
        }
      } else {
        if (handle_disconnections(args))
          broadcast_game_state(args);
        if (args->game_state->player_count == 1)
          return LOOP_BREAK;
      }
    }
  }

  int n_wilds = 0;
  if (received_hand.card[0].face_val != 0) {
    // Apply wilds:
    for (int i = 0; i < MAX_HAND_SIZE; i++) {
      DH_Card c = received_hand.card[i];
      if (!DH_is_card_null(c)) {
        if (args->real_hand->player[id].card[i].face_val == DH_CARD_TWO) {
          args->real_hand->player[id].card[i] = c;
          n_wilds++;
        } else
          fprintf(stderr, "Invalid wild replacement (not a 2)\n");
      }
    }
  }

  if (n_wilds > 0) {
    char status_str[LEN_STATUS_STR] = {0};
    snprintf(status_str, sizeof status_str, "%s exchanged %d wild card(s)",
             args->game_state->player[id].nick, n_wilds);
    broadcast_status_message(args, status_str);
  }

  return LOOP_OK;
}

static EPlayerAction_t handle_check(PlayerActionMsg_t *action) {
  action->str = _("checked");
  return ACTION_CHECK;
}

static EPlayerAction_t handle_fold(GameState_t *game_state, RealHand_t *real_hand, Player_t *turn,
                                   Player_t **starting_turn, PlayerActionMsg_t *action) {
  turn->in = false;
  for (int i = 0; i < MAX_HAND_SIZE; i++) {
    real_hand->player[turn->id].card[i] = DH_card_null;
    turn->hand.card[i] = DH_card_null;
  }
  game_state->player_count--;
  if (game_state->player_count > 1 && turn == *starting_turn)
    *starting_turn = get_next_player(game_state->player, turn->id);
  action->str = _("folded");
  return ACTION_FOLD;
}

static bool has_paid_all_bets(const uint32_t total_paid, const uint32_t total_bets_plus_raises) {
  return total_paid == total_bets_plus_raises;
}

static void determine_winner(ArgsBroadcastGameState_t *args, RoundResults *results) {
  if (results->n_winners > 0)
    return;

  uint8_t pl_count = args->game_state->player_count;

  Player_t *ptr = *args->starting_turn;

  if (args->game_state->deuces_wild) {
    args->game_state->player_exchanging = true;
    broadcast_game_state(args);
    ELoop_t w = LOOP_OK;
    for (uint8_t i = 0; i < pl_count; i++) {
      if (ptr->in) {
        for (int c = 0; c < MAX_HAND_SIZE; c++) {
          if (args->real_hand->player[ptr->id].card[c].face_val == DH_CARD_TWO) {
            args->turn_id = ptr->id;
            // broadcast_game_state(args);
            broadcast_turn_id(args);
            w = handle_wild_cards(args, args->clients[ptr->id], ptr->id);
            break;
          }
        }
      }
      if (w == LOOP_BREAK)
        if (args->game_state->player_count == 1)
          break;
      ptr = get_next_player(args->game_state->player, ptr->id);
    }
  }

  ptr = *args->starting_turn;
  do {
    handle_sort_hand(&args->real_hand->player[ptr->id],
                     args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type);
    ptr = get_next_player(args->game_state->player, ptr->id);
  } while (ptr && ptr != *args->starting_turn);

  // When set to true, the opponents` cards will be revealed to all the players the next
  // time broadcast_game_state is called
  args->game_state->winner_declared = true;

  POKEVAL_NeedComparing *need_comparing = calloc_wrap(pl_count * sizeof(*need_comparing), 1);
  ptr = *args->starting_turn;
  for (uint8_t i = 0; i < pl_count; i++) {
    need_comparing[i].won = false;
    need_comparing[i].id = ptr->id;
    memcpy(&need_comparing[i].hand, &args->real_hand->player[ptr->id], sizeof(POKEVAL_Hand_7));
    ptr = get_next_player(args->game_state->player, ptr->id);
  }

  results->n_winners = POKEVAL_compare_hands(
      need_comparing, pl_count, args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type);
  uint8_t winners = 0;

  uint32_t pot = args->game_state->pot;
  uint32_t share = pot / results->n_winners;
  uint32_t leftover = pot % results->n_winners;
  args->game_state->pot = leftover; // Remainder stays in the pot

  for (int i = 0; i < pl_count; i++) {
    if (!need_comparing[i].won)
      continue;

    results->id[winners++] = need_comparing[i].id;
    Player_t *winner = &args->game_state->player[need_comparing[i].id];
    winner->winner = true;

    char status_str[LEN_STATUS_STR];
    snprintf(status_str, sizeof status_str, "%s wins %" PRIu32 " with %s", winner->nick, share,
             POKEVAL_rank[POKEVAL_evaluate_hand(need_comparing[i].hand_5)]);

    broadcast_status_message(args, status_str);

    if (args->cli_args->server_log_game_results_file) {
      FILE *fp = fopen(args->cli_args->server_log_game_results_file, "a");
      if (!fp)
        perror("fopen");
      else {
        fprintf(fp, "pot: %u<br>\n", pot);
        fprintf(fp, "%s\n\n", status_str);
        fclose(fp);
      }
    }
    winner->coins += share;
  }
  free(need_comparing);
  broadcast_game_state(args);
}

static void award_last_player_in_game(ArgsBroadcastGameState_t *args, Player_t *turn,
                                      RoundResults *results) {
  if (!turn->is_connected || !turn->in) {
    // fprintf(stderr, "turn->id: %d | %d\n", turn->id, __LINE__);
    turn = get_next_player(args->game_state->player, 0);
  }
  turn->winner = true;
  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof(status_str), "%s wins %d", turn->nick, args->game_state->pot);
  broadcast_status_message(args, status_str);
  if (args->cli_args->server_log_game_results_file) {
    FILE *fp = fopen(args->cli_args->server_log_game_results_file, "a");
    if (!fp)
      perror("fopen");
    else {
      fprintf(fp, "pot: %d<br>\n", args->game_state->pot);
      fprintf(fp, "%s\n\n", status_str);
      fclose(fp);
    }
  }

  args->game_state->winner_declared = true;
  results->n_winners = 1;
  // fprintf(stderr, "winner id from fold: %d\n", turn->id);
  results->id[0] = turn->id;
  turn->coins += args->game_state->pot;
  args->game_state->pot = 0;
  return;
}

static RoundResults handle_round_real(ArgsBroadcastGameState_t *args) {
  args->game_state->raises_remaining = args->config->max_raises;
  args->game_state->prev_bet_amount = 0;

  Player_t *turn;
  // printf("%s:turn->id: %d\n", __func__, turn->id);

  RoundResults results = {0};

  turn = *args->starting_turn;
  uint32_t total_bets_plus_raises = 0;
  uint32_t player_total_paid[MAX_PLAYERS] = {0};
  uint8_t num_turns = 0;

  do {
    args->turn_id = turn->id;
    broadcast_turn_id(args);
    broadcast_game_state(args);

    uint32_t wait_ms = args->game_settings->action_timeout_ms;
    uint32_t start = SDL_GetTicks();
    PlayerActionMsg_t action = {0};

    uint16_t opcode = total_bets_plus_raises == 0 ? MSG_BET_CHECK_FOLD : MSG_CALL_RAISE_FOLD;
    if (send_opcode(args->clients[turn->id], opcode) != 0)
      fputs("Error sending action prompt", stderr);

    while (SDL_GetTicks() - start < wait_ms) {
      register_new_client(args);
      // fprintf(stderr, "Waiting for action from %d\n", args->turn_id);
      int n_ready = SDLNet_CheckSockets(args->socket_set, 100); // wait up to 100ms
      if (n_ready > 0) {
        // If this socket is ready (the player who's turn it is), they either
        // disconnected, or have sent an action.
        if (SDLNet_SocketReady(args->clients[turn->id])) {
          // puts("socket ready");
          // char tmp[sizeof args->game_state->status_str];
          if (recv_player_action(args->clients[turn->id], &action) > 0) {
            if (opcode == MSG_BET_CHECK_FOLD) {
              switch (action.action) {
              case ACTION_CHECK:
                handle_check(&action);
                break;
              case ACTION_BET:
                server_handle_bet(args->game_state, &player_total_paid[turn->id], turn->id,
                                  action.amount, &total_bets_plus_raises);
                action.str = _("bet ");
                break;
              case ACTION_FOLD:
                handle_fold(args->game_state, args->real_hand, turn, args->starting_turn, &action);
                break;
              default:
                fprintf(stderr, "Invalid Action received\n");
                exit(EXIT_FAILURE);
              }
            } else {
              switch (action.action) {
              case ACTION_CALL:
                server_handle_call(args->game_state, &player_total_paid[turn->id], turn->id,
                                   &total_bets_plus_raises);
                action.str = _("called");
                break;
              case ACTION_RAISE:
                if (args->game_state->raises_remaining > 0) {
                  server_handle_raise(args->game_state, &player_total_paid[turn->id], turn->id,
                                      action.amount, &total_bets_plus_raises);
                  action.str = _("raised ");
                } else
                  fputs("Raise received; however, max raises has been reached. The client should "
                        "not be able to send a raise\n",
                        stderr);
                break;
              case ACTION_FOLD:
                handle_fold(args->game_state, args->real_hand, turn, args->starting_turn, &action);
                break;
              default:
                fputs("Invalid Action received\nThe client is writing checks their body "
                      "can't cash.\n",
                      stderr);
                remove_disconnected_player(args, turn->id);
              }
            }
          } else {
            remove_disconnected_player(args, args->turn_id);
            break;
          }
          break;
        } else {
          // There is data to be read, but not from the player whos turn it is, so probably
          // a disconnect by another client.
          handle_disconnections(args);
          if (args->game_state->player_count == 1)
            break;
          continue;
        }
      }
    }

    char status_str[LEN_STATUS_STR] = {0};
    if (args->game_state->player_count > 1) {
      if (turn->is_connected) {
        if (action.action == 0) {
          if (!has_paid_all_bets(player_total_paid[turn->id], total_bets_plus_raises)) {
            action.action =
                handle_fold(args->game_state, args->real_hand, turn, args->starting_turn, &action);
          } else if (total_bets_plus_raises == 0) {
            action.action = handle_check(&action);
          }
        }

        if (action.amount > 0)
          snprintf(status_str, sizeof status_str, "%s %s%d", turn->nick, action.str, action.amount);
        else
          snprintf(status_str, sizeof status_str, "%s %s", turn->nick, action.str);
      }

      broadcast_status_message(args, status_str);
      verbose_puts(status_str);

      // player_count might be 1 now, if a player folded due to an action timeout
      if (args->game_state->player_count == 1) {
        // broadcast_game_state(args);
        award_last_player_in_game(args, turn, &results);
        break;
      }

      turn = get_next_player(args->game_state->player, turn->id);
      num_turns++;
      if (num_turns >= args->game_state->player_count) {
        if (total_bets_plus_raises == 0) {
          if (turn == *args->starting_turn)
            break;
        } else if (has_paid_all_bets(player_total_paid[turn->id], total_bets_plus_raises)) {
          break; // Everyone either checked or paid all bets and raises
        }
      }

      if (results.n_winners > 0) {
        break;
      }
    } else {
      if (action.str != NULL) {
        snprintf(status_str, sizeof status_str, "%s %s", turn->nick, action.str);
        broadcast_status_message(args, status_str);
      }
      award_last_player_in_game(args, turn, &results);
      break;
    }
  } while (true);

  return results;
}

static void reset_players(GameState_t *game_state) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    Player_t *player = &game_state->player[i];
    if (!player->is_connected)
      continue;
    player->in = true;
    player->winner = false;
    memset(&player->hand, 0, sizeof(player->hand));
  }
}

static void remove_disconnected_player(ArgsBroadcastGameState_t *args, const int8_t id) {
  Player_t *p = &args->game_state->player[id];
  if (SDLNet_TCP_DelSocket(args->socket_set, args->clients[id]) == -1) {
    fputs(SDLNet_GetError(), stderr);
    return;
  }

  printf("Client %d disconnected\n", id);
  SDLNet_TCP_Close(args->clients[id]);
  args->clients[id] = NULL;
  args->slot_taken[id] = false;

  // Reset player info
  p->coins = args->config->starting_coins;
  p->winner = false;
  p->in = false;
  p->is_connected = false;

  // If the disconnect happened during a game, not at the menu
  if (args->game_state->player_count > 0) {
    args->game_state->player_count--;
    char status_str[LEN_STATUS_STR] = {0};
    snprintf(status_str, sizeof status_str, _("%s disconnected"), p->nick);
    broadcast_status_message(args, status_str);

    if (args->game_state->player_count > 1)
      if ((*args->starting_turn)->id == id)
        *args->starting_turn = get_next_player(args->game_state->player, id);
  }

  memset(p->nick, 0, sizeof(p->nick));
  broadcast_game_state(args);
}

static bool handle_disconnections(ArgsBroadcastGameState_t *args) {
  bool someone_disconnected = false;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!args->slot_taken[i])
      continue;

    if (SDLNet_SocketReady(args->clients[i])) {
      char tmp;
      int result = SDLNet_TCP_Recv(args->clients[i], &tmp, 1);
      if (result <= 0) {
        remove_disconnected_player(args, i);
        someone_disconnected = true;
        // Clear more fields if your struct includes game progress, bet, etc.
      } else {
        // Optional: put `tmp` in a buffer if you want to process it later
        // In this case, it might be better to queue it per client
      }
    }
  }
  return someone_disconnected;
}

static uint8_t count_active_clients(const bool *slot_taken) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    if (slot_taken[i])
      count++;
  return count;
}

static bool reassign_dealer_if_needed(GameState_t *game_state, bool *slot_taken) {
  if (!slot_taken[game_state->dealer_id]) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (slot_taken[i]) {
        game_state->dealer_id = i;
        printf("Dealer reassigned to client %d\n", i);
        return true;
      }
    }
    // No clients left — should not happen in this context
    game_state->dealer_id = -1;
    printf("No players available to assign as dealer.\n");
  }
  return false;
}

static int get_next_dealer(int current, const bool *slot_taken) {
  for (int i = 1; i <= MAX_CLIENTS; i++) {
    int next = (current + i) % MAX_CLIENTS;
    if (slot_taken[next])
      return next;
  }
  return -1; // No valid dealer
}

void game_five_card_draw(GAME_ARGS) {
  server_handle_ante(args->game_state, args->config->ante);

  Player_t *turn = *args->starting_turn;
  do {
    handle_sort_hand(&args->real_hand->player[turn->id],
                     args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type);
    turn = get_next_player(players_array, turn->id);
  } while (turn && turn != *args->starting_turn);

  RoundResults results = {0};

  for (int i = 0; i < choice->n_betting_rounds; i++) {
    results = handle_round();
    if (results.n_winners > 0 || i == choice->n_draws)
      break;

    broadcast_game_state(args);
    turn = *args->starting_turn;

    do {
      args->turn_id = turn->id;
      verbose_printf("turn->id: %d\n", turn->id);

      broadcast_turn_id(args);

      ELoop_t d = handle_draw(args, args->clients[turn->id], turn->id, deck);
      if (d == LOOP_BREAK) {
        // broadcast_game_state(args);
        if (args->game_state->player_count == 1) {
          award_last_player_in_game(args, turn, &results);
          break;
        }
      } else if (d != LOOP_OK)
        printf("Failed to receive cards or player disconnected: %d\n", turn->id);

      turn = get_next_player(players_array, turn->id);
    } while (turn && turn != *args->starting_turn);
    broadcast_game_state(args);
    if (results.n_winners > 0)
      break;
  }
  determine_winner(args, &results);
}

void game_stud(GAME_ARGS) {
  Player_t *turn;
  server_handle_ante(args->game_state, args->config->ante);

  RoundResults results = {0};
  for (int i = 0; i < choice->n_betting_rounds; i++) {
    results = handle_round();

    if (results.n_winners > 0 || i == choice->n_stud_new_cards)
      break;

    turn = *args->starting_turn;

    printf("round: %d\n", i);
    do {
      int id = turn->id;
      POKEVAL_Hand_7 *hand = &turn->hand;

      uint8_t n = i + (choice->hand_size - choice->n_stud_new_cards);
      args->real_hand->player[id].card[n] = DH_deal_top_card(deck);
      if (n != 6)
        hand->card[n] = args->real_hand->player[id].card[n];
      else
        hand->card[n] = DH_card_back;
      // broadcast_game_state(args);
      turn = get_next_player(players_array, turn->id);
    } while (turn && turn != *args->starting_turn);
    broadcast_game_state(args);
  }

  // 7-card stud setup (for testing)
  /*
  args->real_hand->player[0].card[0].face_val = DH_CARD_KING;
  args->real_hand->player[0].card[1].face_val = DH_CARD_KING;
  args->real_hand->player[0].card[2].face_val = DH_CARD_KING;
  args->real_hand->player[0].card[3].face_val = DH_CARD_ACE;
  args->real_hand->player[0].card[4].face_val = DH_CARD_THREE;
  args->real_hand->player[0].card[5].face_val = DH_CARD_JACK;
  args->real_hand->player[0].card[6].face_val = DH_CARD_TWO;
  */

  determine_winner(args, &results);
}

static void play_game(ArgsBroadcastGameState_t *args, DH_Deck *deck) {
  DH_shuffle_deck(deck);
  if (!args->cli_args->test_mode) {
    int cut_point = 16 + pcg32_boundedrand_r(&rng, 21);
    DH_cut_deck(deck, cut_point);
  }

  Player_t *players_array = args->game_state->player;
  *args->real_hand = deal_cards_to_players(args->game_state, deck, args->game_type);

  if (args->cli_args->test_mode) {
    static int test_case = 0;
    test_case++;
    if (test_case == 1) {
      for (int i = 1; i < 3; i++)
        for (int j = 0; j < 4; j++)
          args->real_hand->player[i].card[j].face_val = DH_CARD_ACE;

      args->real_hand->player[1].card[4].face_val = DH_CARD_KING;
      args->real_hand->player[2].card[4].face_val = DH_CARD_KING;
    } else if (test_case == 2) {
      for (int j = 0; j < 4; j++)
        args->real_hand->player[2].card[j].face_val = DH_CARD_ACE;
    } else if (test_case == 3) {
      for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
          args->real_hand->player[i].card[j].face_val = DH_CARD_ACE;

      args->real_hand->player[0].card[4].face_val = DH_CARD_KING;
      args->real_hand->player[1].card[4].face_val = DH_CARD_KING;
      args->real_hand->player[2].card[4].face_val = DH_CARD_KING;
    }
  }

  // args->real_hand->player[0].card[0].face_val = DH_CARD_TWO;
  // args->real_hand->player[0].card[3].face_val = DH_CARD_TWO;

  // args->real_hand->player[0].card[3].face_val = DH_CARD_TWO;
  // args->real_hand->player[1].card[3].face_val = DH_CARD_TWO;
  // args->real_hand->player[2].card[3].face_val = DH_CARD_TWO;

  // Lowball setups
  //
  // args->real_hand->player[0].card[0].face_val = DH_CARD_ACE;
  // args->real_hand->player[0].card[1].face_val = DH_CARD_TWO;
  // args->real_hand->player[0].card[2].face_val = DH_CARD_THREE;
  // args->real_hand->player[0].card[3].face_val = DH_CARD_FOUR;
  // args->real_hand->player[0].card[4].face_val = DH_CARD_SIX;

  // args->real_hand->player[1].card[0].face_val = DH_CARD_TWO;
  // args->real_hand->player[1].card[1].face_val = DH_CARD_THREE;
  // args->real_hand->player[1].card[2].face_val = DH_CARD_FOUR;
  // args->real_hand->player[1].card[3].face_val = DH_CARD_FIVE;
  // args->real_hand->player[1].card[4].face_val = DH_CARD_SIX;
  //
  //  In lowball, 8-5-4-3-2 defeats 9-7-6-4-3
  // args->real_hand->player[0].card[0].face_val = DH_CARD_EIGHT;
  // args->real_hand->player[0].card[1].face_val = DH_CARD_FIVE;
  // args->real_hand->player[0].card[2].face_val = DH_CARD_FOUR;
  // args->real_hand->player[0].card[3].face_val = DH_CARD_THREE;
  // args->real_hand->player[0].card[4].face_val = DH_CARD_TWO;

  // args->real_hand->player[1].card[0].face_val = DH_CARD_NINE;
  // args->real_hand->player[1].card[1].face_val = DH_CARD_SEVEN;
  // args->real_hand->player[1].card[2].face_val = DH_CARD_SIX;
  // args->real_hand->player[1].card[3].face_val = DH_CARD_FOUR;
  // args->real_hand->player[1].card[4].face_val = DH_CARD_THREE;

  args->game_state->winner_declared = false;
  args->game_state->prev_bet_amount = 0;
  args->game_state->player_count = count_active_clients(args->slot_taken);
  verbose_printf("player count: %d\n", args->game_state->player_count);
  args->game_state->winner_declared = false;

  Player_t *turn = get_next_player(players_array, args->game_state->dealer_id);
  args->starting_turn = &turn;
  broadcast_game_state(args);

  const GameChoice_t *choice = find_game_choice_by_type(args->game_type);
  char tmp[LEN_STATUS_STR] = {0};
  snprintf(tmp, sizeof(tmp), _("Game: %s%s"), choice->str,
           args->game_state->deuces_wild ? _(" / Deuces Wild") : "");
  broadcast_status_message(args, tmp);

  if (args->cli_args->server_log_game_results_file) {
    FILE *fp = fopen(args->cli_args->server_log_game_results_file, "a");
    if (!fp)
      perror("fopen");
    else {
      fprintf(fp, "### %s\n\n", tmp);
      Player_t *p = *args->starting_turn;
      do {
        fprintf(fp, "%s: %d<br>\n", args->game_state->player[p->id].nick,
                args->game_state->player[p->id].coins);

        p = get_next_player(players_array, p->id);
      } while (p && p != *args->starting_turn);
      fclose(fp);
    }
  }

  if (choice && choice->func) {
    // Using function pointers...
    choice->func(args, players_array, deck, choice);
  }
}

static size_t utf8_char_len(const char *s) {
  unsigned char c = (unsigned char)*s;
  if (c < 0x80)
    return 1;
  else if ((c >> 5) == 0x6)
    return 2;
  else if ((c >> 4) == 0xE)
    return 3;
  else if ((c >> 3) == 0x1E)
    return 4;
  return 1; // Invalid or overlong UTF-8; treat as 1 byte
}

static void utf8_truncate(char *dest, const char *src, size_t max_bytes) {
  size_t used = 0;
  while (*src) {
    size_t len = utf8_char_len(src);
    if (used + len > max_bytes - 1) // Ensure space for '\0'
      break;
    memcpy(dest + used, src, len);
    used += len;
    src += len;
  }
  dest[used] = '\0';
}

static void ensure_unique_nick(GameState_t *game_state, Player_t *player, const int slot) {
  const size_t max_len = sizeof(player->nick) - 1;
  const int suffix_limit = 1000;

  char base[sizeof(player->nick)];
  utf8_truncate(base, player->nick, sizeof(player->nick));
  base[max_len] = '\0';

  char candidate[sizeof(player->nick)];
  int suffix = 0;

  while (suffix < suffix_limit) {
    if (suffix == 0) {
      strncpy(candidate, base, sizeof(candidate));
      candidate[sizeof(candidate) - 1] = '\0';
    } else {
      int needed = snprintf(NULL, 0, "_%d", suffix);
      size_t base_limit = max_len - needed;

      utf8_truncate(candidate, base, base_limit + 1);
      snprintf(candidate + strlen(candidate), sizeof(candidate) - strlen(candidate), "_%d", suffix);
    }

    bool unique = true;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
      if (i != slot && strcmp(candidate, game_state->player[i].nick) == 0) {
        unique = false;
        break;
      }
    }

    if (unique) {
      strncpy(player->nick, candidate, sizeof(player->nick) - 1);
      player->nick[sizeof(player->nick) - 1] = '\0';
      return;
    }

    suffix++;
  }

  fprintf(stderr, "Could not find a unique nickname after %d attempts\n", suffix_limit);
}

static EReturnCode_t init_game(ArgsBroadcastGameState_t *args, DH_Deck *deck) {

  int8_t *dealer_id = &args->game_state->dealer_id;

  verbose_printf("All %d players are ready. Starting game.\n",
                 count_active_clients(args->slot_taken));
  args->game_state->at_menu = false;

  play_game(args, deck);

  broadcast_game_state(args);

  Uint32 wait_ms = args->game_settings->end_of_game_timeout_ms;
  // Uint32 wait_ms = 2000;
  Uint32 start = SDL_GetTicks();
  while (SDL_GetTicks() - start < wait_ms) {
    register_new_client(args);
    handle_disconnections(args);
  }

  args->game_state->player_count = 0;
  args->game_state->at_menu = true;

  reset_players(args->game_state);

  // Rotate dealer to next active client
  int next_dealer = get_next_dealer(*dealer_id, args->slot_taken);
  if (next_dealer != -1) {
    *dealer_id = next_dealer;
    broadcast_game_state(args);
    verbose_printf("Dealer rotated to player %d\n", next_dealer);
  } else {
    printf("No valid dealer found after rotation\n");
    *dealer_id = -1;
  }
  // broadcast_game_state(args);
  return RC_OK;
}

static int recv_and_validate_protocol_header(TCPsocket sock) {
  verbose_puts("Exchanging protocol information...");
  GameProtocolHeader_t hdr = {0};
  if (recv_all_tcp(sock, &hdr, sizeof(hdr)) <= 0) {
    fprintf(stderr, "Failed to receive protocol header\n");
    return -1;
  }

  if (memcmp(hdr.magic, GAME_PROTOCOL_MAGIC, sizeof(hdr.magic)) != 0) {
    fprintf(stderr, "Protocol magic mismatch: got '%.8s'\n", hdr.magic);
    return -1;
  }

  uint32_t version = SDL_SwapBE16(hdr.version);
  if (version != GAME_PROTOCOL_VERSION) {
    fprintf(stderr, "Unsupported protocol version: %u\n", version);
    return -1;
  }

  return 0; // success
}

static void do_socket_cleanup(TCPsocket sock, SDLNet_SocketSet socket_set, bool *slot_taken,
                              const bool slot, Player_t *p) {
  SDLNet_TCP_DelSocket(socket_set, sock);
  SDLNet_TCP_Close(sock);
  slot_taken[slot] = false;
  if (p) {
    p->is_connected = false;
    p->in = false;
  }
}

static void flush_client_socket(TCPsocket sock) {
  if (!SDLNet_SocketReady(sock))
    return;
  char buffer[512]; // Temp buffer to discard data
  int len;
  SDLNet_SocketSet tmp_set = SDLNet_AllocSocketSet(1);
  SDLNet_TCP_AddSocket(tmp_set, sock);

  // Loop until no more data available (non-blocking read)
  for (;;) {
    if (SDLNet_CheckSockets(tmp_set, 0) <= 0)
      break;

    if (!SDLNet_SocketReady(sock))
      break;

    len = SDLNet_TCP_Recv(sock, buffer, sizeof(buffer));
    if (len <= 0)
      break;

    // fprintf(stderr, "%d\n", __LINE__);
  }
  SDLNet_FreeSocketSet(tmp_set);
}

static ELoop_t register_new_client(ArgsBroadcastGameState_t *args) {
  // checks for and accepts incoming connections
  TCPsocket new_client = SDLNet_TCP_Accept(*args->server_sock);
  if (new_client) {
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!args->slot_taken[i]) {
        slot = i;
        break;
      }
    }

    if (slot != -1) {
      args->clients[slot] = new_client;
      args->slot_taken[slot] = true;
      SDLNet_TCP_AddSocket(args->socket_set, new_client);

      IPaddress *remote_ip = SDLNet_TCP_GetPeerAddress(new_client);
      if (remote_ip) {
        Uint32 ipaddr = SDL_SwapBE32(remote_ip->host);
        Uint16 port = SDL_SwapBE16(remote_ip->port);
        printf("Client %d connected from %d.%d.%d.%d:%d\n", slot, (ipaddr >> 24) & 0xFF,
               (ipaddr >> 16) & 0xFF, (ipaddr >> 8) & 0xFF, ipaddr & 0xFF, port);
      }

      Player_t *slot_id = &(args->game_state->player)[slot];
      slot_id->id = slot;
      slot_id->is_connected = true;
      if (args->game_state->at_menu)
        slot_id->in = true;
      else {
        for (int i = 0; i < MAX_HAND_SIZE; i++)
          args->real_hand->player[slot].card[i] = DH_card_null;
        memcpy(&args->game_state->player[slot].hand, &args->real_hand->player[slot],
               sizeof(POKEVAL_Hand_7));
      }

      if (recv_and_validate_protocol_header(new_client) != 0) {
        do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, NULL);
        return LOOP_CONTINUE;
      }

      if (!args->cli_args->test_mode) {
        Player_t *player = &(args->game_state->player)[slot];
        int16_t net_len;

        // Step 1: Recv the size first (must happen before interpreting it)
        if (recv_all_tcp(new_client, &net_len, sizeof(net_len)) <= 0) {
          // Handle error: client disconnected or invalid
          fprintf(stderr, "Failed to receive nickname length.\n");
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player);
          return LOOP_CONTINUE;
        }

        // Step 2: Now convert
        uint16_t len = SDL_SwapBE16(net_len);

        // Step 3: Validate length
        if (len == 0 || len >= sizeof(player->nick)) {
          fprintf(stderr, "Invalid nickname length: %d\n", len);
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player);
          return LOOP_CONTINUE;
        }

        // Step 4: Read nickname
        memset(player->nick, 0, sizeof(player->nick));
        if (recv_all_tcp(new_client, player->nick, len) != (ssize_t)len) {
          fprintf(stderr, "Failed to receive nickname.\n");
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player);
          return LOOP_CONTINUE;
        }

        // Step 5: Null terminate
        player->nick[len] = '\0';
        verbose_printf("received nick: %s\n", player->nick);
        ensure_unique_nick(args->game_state, player, slot);
      }

      args->game_settings->client_id = slot;
      send_game_settings(args, new_client);
      broadcast_game_state(args);
    } else {
      printf("Server full. Rejecting connection.\n");
      SDLNet_TCP_Close(new_client);
    }
  }
  return LOOP_OK;
}

int run_server(const CliArgs_t *cli_args, Path_t *path) {
  GameState_t game_state = {0};
  ServerConfig_t config = init_game_state(&game_state, path, cli_args);
  GameSettings_t game_settings = init_game_settings(&config, cli_args);
  game_state.pot = 0;

  if (SDL_Init(0) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL or SDL_net init failed: %s\n", SDLNet_GetError());
    return 1;
  }

  IPaddress ip;
  char *host = config.bind_address;
  if (!cli_args->bind_address) {
    // ip.host = SDL_SwapBE32(INADDR_LOOPBACK);  // 127.0.0.1
    // ip.port = SDL_SwapBE16(default_port);
    host = config.bind_address;
    if (strcmp(config.bind_address, "NULL") == 0)
      host = NULL;
  } else
    host = (char *)cli_args->bind_address;
  verbose_printf("Resolving host: %s\n", (host) ? host : "NULL");
  uint16_t port = (cli_args->port != 0) ? cli_args->port : config.port;
  if (SDLNet_ResolveHost(&ip, host, port) == -1) {
    fprintf(stderr, "SDLNet_ResolveHost: %s\n", SDLNet_GetError());
    SDLNet_Quit();
    SDL_Quit();
    return 1;
  }

  TCPsocket server = SDLNet_TCP_Open(&ip);
  if (!server) {
    fprintf(stderr, "SDLNet_TCP_Open: %s\n", SDLNet_GetError());
    SDLNet_Quit();
    SDL_Quit();
    return 1;
  }

  printf("Server listening on ");
  print_ipaddress(&ip);

  TCPsocket clients[MAX_CLIENTS] = {0};
  SDLNet_SocketSet socket_set = SDLNet_AllocSocketSet(MAX_CLIENTS + 1);
  if (!socket_set) {
    fprintf(stderr, "Failed to allocate socket set: %s\n", SDLNet_GetError());
    SDLNet_TCP_Close(server);
    SDLNet_Quit();
    SDL_Quit();
    return 1;
  }

  DH_Deck deck = DH_get_new_deck();

  if (!cli_args->test_mode)
    DH_pcg_srand_auto();
  else
    DH_pcg_srand(1, 1);

  if (cli_args->server_log_game_results_file) {
    FILE *fp = fopen(cli_args->server_log_game_results_file, "a");
    if (!fp)
      perror("fopen");
    else {
      time_t t = time(NULL);
      struct tm tm = *localtime(&t);
      fprintf(fp, "## %04d-%02d-%02d\n\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
      fclose(fp);
    }
  }

  int game_started = 0;
  RealHand_t real_hand = {0};

  bool slot_taken[MAX_CLIENTS] = {false};
  uint32_t dealer_timeout_start = 0;

  uint32_t last_ping_time = SDL_GetTicks();
  uint32_t ping_times[MAX_CLIENTS] = {0};

  while (!game_started) {
    ArgsBroadcastGameState_t args_broadcast_game_state = {
        .clients = clients,
        .socket_set = socket_set,
        .game_state = &game_state,
        .real_hand = &real_hand,
        .slot_taken = slot_taken,
        .cli_args = cli_args,
        .server_sock = &server,
        .game_settings = &game_settings,
        .config = &config,
        .game_type = 0,
        .starting_turn = NULL,
        .turn_id = 0,
    };

    uint8_t active_clients = count_active_clients(slot_taken);
    int8_t *dealer_id = &game_state.dealer_id;

    ELoop_t ret = register_new_client(&args_broadcast_game_state);
    if (ret == LOOP_CONTINUE)
      continue;
    else if (ret == LOOP_BREAK)
      break;

    if (*dealer_id == -1) {
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (slot_taken[i]) {
          *dealer_id = i;
          printf("Initial dealer set to player %d\n", i);
          break;
        }
      }
    }

    active_clients = count_active_clients(slot_taken);
    if (active_clients == 0)
      continue;

    if (active_clients > 0) {
      uint32_t now = SDL_GetTicks();
      if (now - last_ping_time >= 5000) {
        // Send ping requests
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (!clients[i])
            continue;
          if (send_ping_request(clients[i]) < 0) {
            fprintf(stderr, "[PING] Failed to send ping request to client %d\n", i);
          } else
            verbose_printf("[PING] sent to %d\n", i);
        }
        last_ping_time = now;
        // Broadcast ping times
        broadcast_ping_times(&args_broadcast_game_state, ping_times);
      }
      int recv_pings = SDLNet_CheckSockets(socket_set, 50);
      if (recv_pings == -1)
        fputs(SDLNet_GetError(), stderr);
      else if (recv_pings != 0) {
        bool break_loop = false;
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (!clients[i])
            continue;

          if (!SDLNet_SocketReady(clients[i]))
            continue;

          // Read the message size first (4 bytes)
          uint32_t size_net = 0;
          if (recv_all_tcp(clients[i], &size_net, sizeof(size_net)) <= 0) {
            fprintf(stderr, "[NET] Disconnection while reading size from client %d\n", i);
            remove_disconnected_player(&args_broadcast_game_state, i);
            continue;
          }

          uint32_t size = SDL_SwapBE32(size_net);
          if (size == 0 || size > 65536) {
            fprintf(stderr, "[NET] Invalid message size from client %d: %u\n", i, size);
            continue;
          }

          // Read the payload (size bytes)
          uint8_t *buffer = malloc(size);
          if (!buffer) {
            fprintf(stderr, "[NET] Memory allocation failed for client %d\n", i);
            continue;
          }

          if (recv_all_tcp(clients[i], buffer, size) <= 0) {
            fprintf(stderr, "[NET] Disconnection while reading payload from client %d\n", i);
            free(buffer);
            continue;
          }

          // This breaks on FreeBSD, for example, the server will
          // incorrectly receive 0100 instead of the correct 0001
          // uint16_t opcode = (buffer[0] << 8) | buffer[1];
          //
          // Use this instead (also used in net.c).
          uint16_t opcode_be;
          memcpy(&opcode_be, buffer, sizeof(opcode_be));
          uint16_t opcode = SDL_SwapBE16(opcode_be);
          switch (opcode) {
          case MSG_PING_RESPONSE: {
            PingResponse *resp = ping_response__unpack(NULL, size - 2, buffer + 2);
            if (!resp) {
              fprintf(stderr, "[PING] Failed to unpack PingResponse from client %d\n", i);
            } else {
              now = SDL_GetTicks();
              ping_times[i] = now - resp->timestamp;
              ping_response__free_unpacked(resp, NULL);
            }
            break;
          }

          case MSG_GAME_SELECT: {
            if (active_clients == 1) {
              fputs("The dealer sent a game but this option should be\n"
                    "disabled when there is only one active client\n",
                    stderr);
              break;
            }

            // Size check — includes opcode in this context
            if (size != OPCODE_SIZE + sizeof(GameSelectPayload_t)) {
              fprintf(stderr,
                      "[NET] Invalid MSG_GAME_SELECT size from client %d "
                      "(got %zu, expected %zu)\n",
                      i, (size_t)size, (size_t)(OPCODE_SIZE + sizeof(GameSelectPayload_t)));
              break;
            }

            // Read payload directly after the opcode
            GameSelectPayload_t payload;
            memcpy(&payload, buffer + OPCODE_SIZE, sizeof(payload));

            args_broadcast_game_state.game_type = payload.game_type;
            game_state.deuces_wild = (payload.deuces_wild != 0);

            if (i == *dealer_id) {
              verbose_printf("Dealer selected game: %d (deuces wild: %d)\n", payload.game_type,
                             payload.deuces_wild);

              break_loop = true;
              if (!cli_args->test_mode) {
                int ping_discards;
                while ((ping_discards = SDLNet_CheckSockets(socket_set, PING_THRESHOLD)) != 0) {
                  if (ping_discards == -1) {
                    fputs(SDLNet_GetError(), stderr);
                    break;
                  }

                  for (int d = 0; d < MAX_CLIENTS; d++) {
                    if (!clients[d])
                      continue;
                    flush_client_socket(clients[d]);
                    // fprintf(stderr, "%d\n", __LINE__);
                  }
                }
              }

              init_game(&args_broadcast_game_state, &deck);
              dealer_timeout_start = 0;
            } else {
              fprintf(stderr, "Non-dealer client %d sent MSG_GAME_SELECT (ignored)\n", i);
            }
            break;
          }

          default:
            // Ignore or log
            fprintf(stderr, "[NET] Unknown opcode %04X from client %d\n", opcode, i);
            break;
          }

          free(buffer);
          if (break_loop)
            break;
        }
      }
    }

    if (active_clients > 1) {
      if (dealer_timeout_start == 0) {
        dealer_timeout_start = SDL_GetTicks();
      } else if (SDL_GetTicks() - dealer_timeout_start >= config.dealer_timeout_ms) {
        *dealer_id = get_next_dealer(*dealer_id, slot_taken);
        dealer_timeout_start = 0;
        broadcast_game_state(&args_broadcast_game_state);
      }
    } else if (active_clients == 1)
      dealer_timeout_start = 0;

    if (reassign_dealer_if_needed(&game_state, slot_taken))
      broadcast_game_state(&args_broadcast_game_state);
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (slot_taken[i]) {
      SDLNet_TCP_Close(clients[i]);
    }
  }

  SDLNet_TCP_Close(server);
  SDLNet_FreeSocketSet(socket_set);
  SDLNet_Quit();
  SDL_Quit();

  return 0;
}
