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
#include "globals.h"
#include "server.h"
#include "util.h"

#include <sodium.h>

#define MAX_DISCARDS 4
#define MAX_WILDS 4

#define SEND_RETRY_COUNT 3
#define SEND_RETRY_DELAY_MS 500
#define PING_THRESHOLD 1000

#define handle_round() handle_round_real(args, 0, -1)
#define handle_round_bringin(amt, paid_id) handle_round_real(args, (amt), (paid_id))

typedef struct {
  uint8_t n_winners;
  int id[MAX_PLAYERS];
} RoundResults;

typedef struct {
  uint8_t discard_count;
  uint8_t discard_indices[MAX_DISCARDS];
} DrawRequestMsg_t;

static void remove_disconnected_player(ArgsBroadcastGameState_t *args, const int8_t id);

static void kick_player(ArgsBroadcastGameState_t *args, int8_t id);
static void ban_player(ArgsBroadcastGameState_t *args, int8_t id);
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

  for (int8_t i = 0; i < MAX_PLAYERS; i++) {
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
  return config;
}

GameSettings_t init_game_settings(const ServerConfig_t *config, const CliArgs_t *cli_args) {
  GameSettings_t game_settings = {
      .action_timeout_ms = config->action_timeout_ms,
      .end_of_game_timeout_ms = (cli_args->test_mode) ? 500 : config->end_of_game_timeout_ms,
      .bet_amount_count = config->bet_amount_count,
  };
  memcpy(game_settings.bet_amounts, config->bet_amounts,
         config->bet_amount_count * sizeof(uint32_t));
  return game_settings;
}

// In the future, hands will be sent using functions like this, rather than how it's
// presently done in broadcast_game_state()
static int send_new_hand(TCPsocket sock, const POKEVAL_Hand_9 *hand, uint8_t hand_size) {
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
  uint32_t payload_size = OPCODE_SIZE + (uint32_t)packed_size;
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
  if (!choice) {
    fprintf(stderr, "deal_cards_to_players: unknown game_type 0x%02x\n", game_type);
    return real_hand;
  }

  for (int i = 0; i < MAX_HAND_SIZE; i++) {
    turn = starting_turn;
    do {
      if (i >= choice->n_cards_initial_deal) {
        real_hand.player[turn->id].card[i] = DH_card_null;
        turn->hand.card[i] = DH_card_null;
      } else {
        real_hand.player[turn->id].card[i] = DH_deal_top_card(deck);
        if (choice->card_slot[i] == CARD_SLOT_FACE_UP ||
            choice->card_slot[i] == CARD_SLOT_COMMUNITY)
          turn->hand.card[i] = real_hand.player[turn->id].card[i];
        else
          turn->hand.card[i] = DH_card_back;
      }
      turn = get_next_player(players_array, turn->id);
    } while (turn && turn != starting_turn);
  }

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

// At showdown all hands are revealed — required for winner highlighting and
// consistent with poker rules. Called once before the per-client send loop so
// every client receives the same fully-revealed game state.
static void reveal_all_hands(GameState_t *game_state, const RealHand_t *real_hand) {
  for (int j = 0; j < MAX_PLAYERS; j++)
    memcpy(&game_state->player[j].hand, &real_hand->player[j], sizeof(POKEVAL_Hand_9));
}

static void broadcast_game_state(ArgsBroadcastGameState_t *args) {
  if (args->game_state->winner_declared && args->game_state->player_count != 1)
    reveal_all_hands(args->game_state, args->real_hand);

  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (!args->clients[i]) {
      // fprintf(stderr, "skipping %d\n", i);
      continue;
    }

    // During play each client receives only their own real cards; all other
    // players' hole cards are sent as backs (anti-cheat).
    POKEVAL_Hand_9 hand_tmp = {0};
    bool mask_opponents = !args->game_state->winner_declared || args->game_state->player_count == 1;
    if (mask_opponents) {
      memcpy(&hand_tmp, &args->game_state->player[i].hand, sizeof(POKEVAL_Hand_9));
      memcpy(&args->game_state->player[i].hand, &args->real_hand->player[i],
             sizeof(POKEVAL_Hand_9));
    }

    uint32_t size = 0;
    uint8_t *data = serialize_game_state(args->game_state, &size);
    if (!data)
      return;

    if (mask_opponents)
      memcpy(&args->game_state->player[i].hand, &hand_tmp, sizeof(POKEVAL_Hand_9));

    uint32_t size_net = SDL_SwapBE32((uint32_t)size);

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

  uint32_t size_net = SDL_SwapBE32((uint32_t)size);

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

  uint32_t size = SDL_SwapBE32(2 + (uint32_t)msg_len); // payload: 2-byte opcode + N-byte msg
  uint8_t buffer[4 + 2 + LEN_STATUS_STR]; // max: 4 bytes (size) + 2 (opcode) + 100 (msg)

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

static void broadcast_game_type(const ArgsBroadcastGameState_t *args) {
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

    if (send_game_select(sock, args->game_type, args->deuces_wild) < 0) {
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

typedef enum {
  TURN_MSG_ACTION,   /* MSG_PLAYER_ACTION — game action from the turn player */
  TURN_MSG_KICK_BAN, /* MSG_KICK_PLAYER / MSG_BAN_PLAYER from an admin who happens to be on turn */
  TURN_MSG_DISCONNECT, /* connection closed or unrecognised data */
} ETurnMsg_t;

/*
 * Read one message from the turn player's socket and classify it.
 *
 * Player-action messages are 7 bytes with no length prefix:
 *   [0-1] opcode (MSG_PLAYER_ACTION)  [2] action  [3-6] amount
 *
 * Kick/ban messages are sent via send_message() which prepends a 4-byte BE
 * length, giving the same 7-byte total on the wire:
 *   [0-3] length (BE, == 3)  [4-5] opcode  [6] target_id
 *
 * We tell them apart by checking bytes [0-1]: MSG_PLAYER_ACTION == 0x0002,
 * whereas the length prefix for a 1-byte kick/ban payload starts with 0x00 0x00.
 */
static ETurnMsg_t recv_turn_player_msg(TCPsocket sock, PlayerActionMsg_t *out_action,
                                       uint16_t *out_kb_opcode, int8_t *out_target_id) {
  uint8_t buf[7];
  if (recv_all_tcp(sock, buf, sizeof(buf)) <= 0)
    return TURN_MSG_DISCONNECT;

  uint16_t opcode = (buf[0] << 8) | buf[1];

  if (opcode == MSG_PLAYER_ACTION) {
    out_action->action = buf[2];
    out_action->amount = ((uint32_t)buf[3] << 24) | ((uint32_t)buf[4] << 16) |
                         ((uint32_t)buf[5] << 8) | (uint32_t)buf[6];
    verbose_printf("Received action %u with amount %" PRIu32 "\n", out_action->action,
                   out_action->amount);
    return TURN_MSG_ACTION;
  }

  /* A kick/ban message starts with a 4-byte BE length (== 3 for a 1-byte
   * payload). The real opcode is at bytes [4-5] and target_id at byte [6]. */
  if (opcode == 0x0000) {
    uint16_t kb_opcode = (buf[4] << 8) | buf[5];
    if (kb_opcode == MSG_KICK_PLAYER || kb_opcode == MSG_BAN_PLAYER) {
      *out_kb_opcode = kb_opcode;
      *out_target_id = (int8_t)buf[6];
      return TURN_MSG_KICK_BAN;
    }
  }

  fprintf(stderr, "[recv_turn_player_msg] Unrecognised opcode 0x%04X\n", opcode);
  return TURN_MSG_DISCONNECT;
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
  uint8_t buf[16]; // ample for one uint32 varint field
  ping_request__pack(&req, buf);

  return send_message(sock, MSG_PING_REQUEST, buf, len);
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

static void handle_sort_hand(POKEVAL_Hand_9 *real_hand, const bool is_lowball,
                             const bool deuces_wild) {
  /* When deuces are wild, select the best 5-card hand using the wild-aware
   * evaluator so that a 2 is never dropped in favour of a higher non-wild
   * kicker when picking from a 6- or 7-card stud hand. */
  POKEVAL_Hand_5 tmp_hand = deuces_wild ? POKEVAL_hand5_from_hand7_wild(real_hand, DH_CARD_TWO)
                                        : POKEVAL_hand5_from_hand7(real_hand);
  if (!is_lowball)
    POKEVAL_sort_hand(&tmp_hand);
  else
    POKEVAL_sort_hand_lowball(&tmp_hand);
  memcpy(&real_hand->card[0], &tmp_hand.card[0], sizeof(tmp_hand.card));

  /* Ensure unused card slots are NULL */
  real_hand->card[5] = DH_card_null;
  real_hand->card[6] = DH_card_null;
  real_hand->card[7] = DH_card_null;
  real_hand->card[8] = DH_card_null;
}

static ELoop_t handle_draw(ArgsBroadcastGameState_t *args, TCPsocket sock, const int8_t id,
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
    int num_ready = SDLNet_CheckSockets(args->socket_set, 10);
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
                     args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type,
                     args->deuces_wild);
    send_new_hand(sock, &args->real_hand->player[id], MAX_HAND_SIZE);
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

  /* Do not truncate or sort hands here — clients display all dealt cards and
   * identify the best 5 themselves.  POKEVAL_compare_hands* already handles
   * 6- and 7-card hands via hand5_from_hand7 internally. */

  // When set to true, the opponents` cards will be revealed to all the players the next
  // time broadcast_game_state is called
  args->game_state->winner_declared = true;

  POKEVAL_NeedComparing *need_comparing = calloc_wrap(pl_count * sizeof(*need_comparing), 1);
  ptr = *args->starting_turn;
  for (uint8_t i = 0; i < pl_count; i++) {
    need_comparing[i].won = false;
    need_comparing[i].id = ptr->id;
    memcpy(&need_comparing[i].hand, &args->real_hand->player[ptr->id], sizeof(POKEVAL_Hand_9));
    ptr = get_next_player(args->game_state->player, ptr->id);
  }

  bool lowball = args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type;
  if (args->game_type == game_choices[OMAHA].game_type)
    results->n_winners = POKEVAL_compare_hands_omaha(need_comparing, pl_count);
  else if (args->deuces_wild)
    results->n_winners = POKEVAL_compare_hands_wild(need_comparing, pl_count, DH_CARD_TWO);
  else
    results->n_winners = POKEVAL_compare_hands(need_comparing, pl_count, lowball);
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

    short hand_rank = args->deuces_wild
                          ? POKEVAL_evaluate_hand_wild(need_comparing[i].hand_5, DH_CARD_TWO)
                          : POKEVAL_evaluate_hand(need_comparing[i].hand_5);
    char status_str[LEN_STATUS_STR];
    snprintf(status_str, sizeof status_str, "%s wins %" PRIu32 " with %s", winner->nick, share,
             POKEVAL_rank[hand_rank]);

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
    if (!turn)
      return;
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

static RoundResults handle_round_real(ArgsBroadcastGameState_t *args, uint32_t initial_bet,
                                      int8_t initial_paid_id) {
  args->game_state->raises_remaining = args->config->max_raises;
  args->game_state->prev_bet_amount = initial_bet;

  Player_t *turn;

  RoundResults results = {0};

  turn = *args->starting_turn;
  uint32_t total_bets_plus_raises = initial_bet;
  uint32_t player_total_paid[MAX_PLAYERS] = {0};
  if (initial_paid_id >= 0 && initial_paid_id < MAX_PLAYERS)
    player_total_paid[initial_paid_id] = initial_bet;
  uint8_t num_turns = 0;

  do {
    args->turn_id = turn->id;
    broadcast_turn_id(args);
    broadcast_game_state(args);

    uint32_t wait_ms = args->game_settings->action_timeout_ms;
    uint32_t start = SDL_GetTicks();
    PlayerActionMsg_t action = {0};

    uint32_t owed = (player_total_paid[turn->id] < total_bets_plus_raises)
                        ? total_bets_plus_raises - player_total_paid[turn->id]
                        : 0;
    // In the bring-in round (initial_bet > 0) and before anyone has completed or
    // raised (total == initial_bet), use the more accurate Complete opcodes.
    bool bringin_round_unopened = (initial_bet > 0 && total_bets_plus_raises == initial_bet);
    uint16_t opcode;
    if (owed > 0)
      opcode = bringin_round_unopened ? MSG_CALL_COMPLETE_FOLD : MSG_CALL_RAISE_FOLD;
    else
      opcode = bringin_round_unopened ? MSG_COMPLETE_CHECK_FOLD : MSG_BET_CHECK_FOLD;
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
          uint16_t kb_opcode = 0;
          int8_t kb_target = -1;
          ETurnMsg_t msg_type =
              recv_turn_player_msg(args->clients[turn->id], &action, &kb_opcode, &kb_target);
          if (msg_type == TURN_MSG_KICK_BAN) {
            /* Admin is on-turn and sent a kick/ban instead of a game action.
             * Process it and keep waiting for their game action. */
            if (kb_target >= 0 && kb_target != turn->id) {
              if (kb_opcode == MSG_KICK_PLAYER)
                kick_player(args, kb_target);
              else
                ban_player(args, kb_target);
              broadcast_game_state(args);
            }
            continue;
          } else if (msg_type == TURN_MSG_ACTION) {
            if (opcode == MSG_BET_CHECK_FOLD || opcode == MSG_COMPLETE_CHECK_FOLD) {
              switch (action.action) {
              case ACTION_CHECK:
                handle_check(&action);
                break;
              case ACTION_BET:
                // Completing the bring-in (COMPLETE_CHECK_FOLD) or opening a new
                // bet (BET_CHECK_FOLD) both count against the raise cap when there
                // is already money in the pot.
                if (total_bets_plus_raises > 0 && args->game_state->raises_remaining > 0)
                  args->game_state->raises_remaining--;
                server_handle_bet(args->game_state, &player_total_paid[turn->id], turn->id,
                                  action.amount, &total_bets_plus_raises);
                action.str = (opcode == MSG_COMPLETE_CHECK_FOLD) ? _("completed ") : _("bet ");
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
              case ACTION_BET:
                // Complete: raise to full bet from the bring-in (MSG_CALL_COMPLETE_FOLD).
                if (opcode == MSG_CALL_COMPLETE_FOLD && args->game_state->raises_remaining > 0) {
                  args->game_state->raises_remaining--;
                  server_handle_bet(args->game_state, &player_total_paid[turn->id], turn->id,
                                    action.amount, &total_bets_plus_raises);
                  action.str = _("completed ");
                } else {
                  fputs("BET received unexpectedly; ignoring\n", stderr);
                }
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
          } else { /* TURN_MSG_DISCONNECT */
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
          args->player_timeouts[turn->id]++;
          if (!has_paid_all_bets(player_total_paid[turn->id], total_bets_plus_raises)) {
            action.action =
                handle_fold(args->game_state, args->real_hand, turn, args->starting_turn, &action);
          } else if (owed == 0) {
            action.action = handle_check(&action);
          }
          const uint8_t m = args->config->action_timeout_max;
          if (m != 0 && !args->cli_args->disable_timeout && args->player_timeouts[turn->id] == m) {
            remove_disconnected_player(args, args->turn_id);
            printf("exceeded timeout threshold (%d): disconnecting %s\n", m, turn->nick);
          }
        } else
          args->player_timeouts[turn->id] = 0;

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

static void kick_player(ArgsBroadcastGameState_t *args, int8_t id) {
  if (id < 0 || id >= MAX_CLIENTS || !args->slot_taken[id])
    return;
  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof status_str, _("%s was kicked"), args->game_state->player[id].nick);
  remove_disconnected_player(args, id);
  broadcast_status_message(args, status_str);
  broadcast_game_state(args);
}

static void ban_player(ArgsBroadcastGameState_t *args, int8_t id) {
  if (id < 0 || id >= MAX_CLIENTS || !args->slot_taken[id])
    return;
  if (args->ban_count < (int)(sizeof(args->ban_list) / sizeof(args->ban_list[0]))) {
    IPaddress *remote_ip = SDLNet_TCP_GetPeerAddress(args->clients[id]);
    if (remote_ip) {
      args->ban_list[args->ban_count++] = remote_ip->host;
      printf("Banned IP: %u\n", remote_ip->host);
    }
  }
  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof status_str, _("%s was banned"), args->game_state->player[id].nick);
  remove_disconnected_player(args, id);
  broadcast_status_message(args, status_str);
  broadcast_game_state(args);
}

static bool handle_disconnections(ArgsBroadcastGameState_t *args) {
  bool someone_disconnected = false;
  for (int8_t i = 0; i < MAX_CLIENTS; i++) {
    if (!args->slot_taken[i])
      continue;
    if (!SDLNet_SocketReady(args->clients[i]))
      continue;

    /* Read the length prefix to determine if this is a disconnect or a message. */
    uint32_t len_be;
    int r = SDLNet_TCP_Recv(args->clients[i], &len_be, sizeof(len_be));
    if (r <= 0) {
      remove_disconnected_player(args, i);
      someone_disconnected = true;
      continue;
    }

    uint32_t msg_len = SDL_SwapBE32(len_be);
    if (msg_len < OPCODE_SIZE || msg_len > 256) {
      remove_disconnected_player(args, i);
      someone_disconnected = true;
      continue;
    }

    uint16_t opcode_be;
    r = SDLNet_TCP_Recv(args->clients[i], &opcode_be, sizeof(opcode_be));
    if (r <= 0) {
      remove_disconnected_player(args, i);
      someone_disconnected = true;
      continue;
    }
    uint16_t opcode = SDL_SwapBE16(opcode_be);

    uint32_t payload_len = msg_len - OPCODE_SIZE;
    uint8_t payload[32] = {0};
    if (payload_len > 0) {
      r = SDLNet_TCP_Recv(args->clients[i], payload,
                          payload_len < sizeof(payload) ? payload_len : sizeof(payload));
      if (r <= 0) {
        remove_disconnected_player(args, i);
        someone_disconnected = true;
        continue;
      }
    }

    if (!args->game_state->player[i].is_admin || payload_len < 1)
      continue;

    int8_t target_id = (int8_t)payload[0];
    if (target_id == i) /* admin can't kick/ban themselves */
      continue;

    if (opcode == MSG_KICK_PLAYER)
      kick_player(args, target_id);
    else if (opcode == MSG_BAN_PLAYER)
      ban_player(args, target_id);
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
  if (game_state->dealer_id < 0 || !slot_taken[game_state->dealer_id]) {
    for (int8_t i = 0; i < MAX_CLIENTS; i++) {
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

static int8_t get_next_dealer(int8_t current, const bool *slot_taken) {
  for (int8_t i = 1; i <= MAX_CLIENTS; i++) {
    int8_t next = (current + i) % MAX_CLIENTS;
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
                     args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type,
                     args->deuces_wild);
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

// Returns the index of the first face-up card within the initial deal, or -1 if none.
static int stud_upcard_idx(const GameChoice_t *choice) {
  for (int i = 0; i < choice->n_cards_initial_deal; i++)
    if (choice->card_slot[i] == CARD_SLOT_FACE_UP)
      return i;
  return -1;
}

// Returns the player whose upcard at real_hand[][upcard_idx] is lowest (bring-in player).
static Player_t *stud_find_bringin_player(const ArgsBroadcastGameState_t *args,
                                          Player_t *players_array, int upcard_idx) {
  Player_t *bringin = NULL;
  DH_Card bringin_card = DH_card_null;

  Player_t *turn = *args->starting_turn;
  do {
    if (!turn->in || !turn->is_connected) {
      turn = get_next_player(players_array, turn->id);
      continue;
    }
    DH_Card upcard = args->real_hand->player[turn->id].card[upcard_idx];
    if (DH_is_card_null(upcard)) {
      turn = get_next_player(players_array, turn->id);
      continue;
    }
    if (bringin == NULL || POKEVAL_card_bringin_lt(upcard, bringin_card)) {
      bringin = turn;
      bringin_card = upcard;
    }
    turn = get_next_player(players_array, turn->id);
  } while (turn && turn != *args->starting_turn);

  return bringin;
}

// Returns the player with the best visible upcard hand at a given street index.
// street_idx is the loop variable i (deal index about to be made); visible cards are
// 0..street_idx-1 where face_up[j] == true.
static Player_t *stud_find_best_upcard_player(const ArgsBroadcastGameState_t *args,
                                              Player_t *players_array, const GameChoice_t *choice,
                                              uint8_t street_idx) {
  Player_t *best_player = NULL;
  uint64_t best_score = 0;

  Player_t *turn = *args->starting_turn;
  do {
    if (!turn->in || !turn->is_connected) {
      turn = get_next_player(players_array, turn->id);
      continue;
    }

    // Collect this player's visible (face-up) cards dealt so far
    DH_Card visible[4];
    int n_visible = 0;
    for (int j = 0; j < street_idx && n_visible < 4; j++) {
      if (choice->card_slot[j] == CARD_SLOT_FACE_UP)
        visible[n_visible++] = args->real_hand->player[turn->id].card[j];
    }

    uint64_t score = POKEVAL_score_stud_upcards(visible, n_visible);
    if (best_player == NULL || score > best_score) {
      best_score = score;
      best_player = turn;
    }

    turn = get_next_player(players_array, turn->id);
  } while (turn && turn != *args->starting_turn);

  return best_player;
}

void game_stud(GAME_ARGS) {
  Player_t *turn;
  server_handle_ante(args->game_state, args->config->ante);

  RoundResults results = {0};
  bool first_round = true;

  // Find the one face-up card in the initial deal — used to determine the bring-in player.
  int upcard_idx = stud_upcard_idx(choice);

  for (uint8_t i = choice->n_cards_initial_deal; i <= choice->hand_size; i++) {
    if (first_round) {
      // Bring-in: player with the lowest upcard posts a forced partial bet.
      Player_t *bringin =
          (upcard_idx >= 0) ? stud_find_bringin_player(args, players_array, upcard_idx) : NULL;

      if (bringin && args->config->bringin_amount > 0) {
        args->turn_id = bringin->id;
        bringin->coins -= (int32_t)args->config->bringin_amount;
        args->game_state->pot += args->config->bringin_amount;

        char bringin_str[LEN_STATUS_STR] = {0};
        snprintf(bringin_str, sizeof(bringin_str), _("%s brings in for %u"), bringin->nick,
                 args->config->bringin_amount);
        broadcast_status_message(args, bringin_str);

        // Action starts with the player to the left of the bring-in player.
        *args->starting_turn = get_next_player(players_array, bringin->id);
        broadcast_game_state(args);
        results = handle_round_bringin(args->config->bringin_amount, bringin->id);
      } else {
        results = handle_round();
      }
      first_round = false;
    } else {
      // Subsequent streets: player with the best visible hand acts first.
      Player_t *best = stud_find_best_upcard_player(args, players_array, choice, i);
      if (best)
        *args->starting_turn = best;
      results = handle_round();
    }

    if (results.n_winners > 0 || i == choice->hand_size)
      break;

    turn = *args->starting_turn;

    verbose_printf("round: %d\n", i);
    do {
      int id = turn->id;
      POKEVAL_Hand_9 *hand = &turn->hand;

      args->real_hand->player[id].card[i] = DH_deal_top_card(deck);
      if (choice->card_slot[i] == CARD_SLOT_FACE_UP)
        hand->card[i] = args->real_hand->player[id].card[i];
      else
        hand->card[i] = DH_card_back;
      turn = get_next_player(players_array, turn->id);
    } while (turn && turn != *args->starting_turn);
    broadcast_game_state(args);
  }

  determine_winner(args, &results);
}

static void deal_community_cards(ArgsBroadcastGameState_t *args, Player_t *players_array,
                                 DH_Deck *deck, uint8_t start_pos, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    uint8_t pos = start_pos + i;
    DH_Card card = DH_deal_top_card(deck);
    for (int p = 0; p < MAX_PLAYERS; p++) {
      if (!players_array[p].is_connected)
        continue;
      args->real_hand->player[p].card[pos] = card;
      players_array[p].hand.card[pos] = card;
    }
  }
}

void game_texas_holdem(GAME_ARGS) {
  (void)choice;
  server_handle_ante(args->game_state, args->config->ante);

  RoundResults results = {0};

  /* Pre-flop betting (2 hole cards already dealt by play_game) */
  results = handle_round();
  if (results.n_winners > 0)
    goto done;

  /* Flop (3 cards), Turn (1), River (1) */
  const uint8_t street_start[] = {2, 5, 6};
  const uint8_t street_count[] = {3, 1, 1};
  for (size_t s = 0; s < ARRAY_SIZE(street_start); s++) {
    deal_community_cards(args, players_array, deck, street_start[s], street_count[s]);
    broadcast_game_state(args);
    results = handle_round();
    if (results.n_winners > 0)
      goto done;
  }

done:
  determine_winner(args, &results);
}

void game_omaha(GAME_ARGS) {
  (void)choice;
  server_handle_ante(args->game_state, args->config->ante);

  RoundResults results = {0};

  /* Pre-flop betting (4 hole cards already dealt by play_game) */
  results = handle_round();
  if (results.n_winners > 0)
    goto done;

  /* Flop (3 cards), Turn (1), River (1) — community cards start at position 4 */
  const uint8_t street_start[] = {4, 7, 8};
  const uint8_t street_count[] = {3, 1, 1};
  for (size_t s = 0; s < ARRAY_SIZE(street_start); s++) {
    deal_community_cards(args, players_array, deck, street_start[s], street_count[s]);
    broadcast_game_state(args);
    results = handle_round();
    if (results.n_winners > 0)
      goto done;
  }

done:
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
  broadcast_game_type(args);

  const GameChoice_t *choice = find_game_choice_by_type(args->game_type);

  if (args->cli_args->server_log_game_results_file) {
    char tmp[LEN_STATUS_STR] = {0};
    snprintf(tmp, sizeof(tmp), _("Game: %s%s"), choice->str,
             args->deuces_wild ? " / Deuces Wild" : "");
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
    SDL_Delay(10);
  }

  args->game_state->player_count = 0;
  args->game_state->at_menu = true;

  reset_players(args->game_state);

  // Rotate dealer to next active client
  int8_t next_dealer = get_next_dealer(*dealer_id, args->slot_taken);
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

static int recv_and_validate_protocol_header(TCPsocket sock, uint8_t *flags_out) {
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
    uint8_t nack = 1;
    send_all_tcp(sock, &nack, sizeof(nack)); // best-effort; we're closing anyway
    return -1;
  }

  uint8_t ack = 0;
  if (send_all_tcp(sock, &ack, sizeof(ack)) != 0) {
    fprintf(stderr, "Failed to send protocol ACK\n");
    return -1;
  }

  *flags_out = hdr.flags;
  return 0; // success
}

static void do_socket_cleanup(TCPsocket sock, SDLNet_SocketSet socket_set, bool *slot_taken,
                              const int slot, Player_t *p) {
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

static int send_nonce(TCPsocket sock, unsigned char nonce[NONCE_SIZE]) {
  randombytes_buf(nonce, NONCE_SIZE);
  return send_all_tcp(sock, nonce, NONCE_SIZE);
}

static int verify_client_password(TCPsocket sock, const char *stored_password,
                                  const unsigned char nonce[NONCE_SIZE]) {
  unsigned char client_hash[HASH_SIZE];

  if (recv_all_tcp(sock, client_hash, HASH_SIZE) < 0)
    return -1;

  unsigned char expected_hash[HASH_SIZE];
  crypto_hash_sha256_state state;

  crypto_hash_sha256_init(&state);
  crypto_hash_sha256_update(&state, (const unsigned char *)stored_password,
                            strlen(stored_password));
  crypto_hash_sha256_update(&state, nonce, NONCE_SIZE);
  crypto_hash_sha256_final(&state, expected_hash);

  if (sodium_memcmp(client_hash, expected_hash, HASH_SIZE) == 0)
    return 0;

  return -1;
}

static ELoop_t register_new_client(ArgsBroadcastGameState_t *args) {
  // checks for and accepts incoming connections
  TCPsocket new_client = SDLNet_TCP_Accept(*args->server_sock);
  if (new_client) {
    IPaddress *peer_ip = SDLNet_TCP_GetPeerAddress(new_client);
    if (peer_ip) {
      for (int b = 0; b < args->ban_count; b++) {
        if (args->ban_list[b] == peer_ip->host) {
          printf("Rejected banned client\n");
          SDLNet_TCP_Close(new_client);
          return LOOP_CONTINUE;
        }
      }
    }

    int8_t slot = -1;
    for (int8_t i = 0; i < MAX_CLIENTS; i++) {
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
      args->player_timeouts[slot] = 0;
      if (args->game_state->at_menu)
        slot_id->in = true;
      else {
        for (int i = 0; i < MAX_HAND_SIZE; i++)
          args->real_hand->player[slot].card[i] = DH_card_null;
        memcpy(&args->game_state->player[slot].hand, &args->real_hand->player[slot],
               sizeof(POKEVAL_Hand_9));
      }

      uint8_t proto_flags = 0;
      if (recv_and_validate_protocol_header(new_client, &proto_flags) != 0) {
        do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, NULL);
        return LOOP_CONTINUE;
      }

      if (!args->cli_args->test_mode) {
        Player_t *player = &(args->game_state->player)[slot];

        bool is_bot = (proto_flags & PROTO_FLAG_BOT) != 0;
        const char *password = args->config->password;

        if (is_bot && !*password) {
          printf("Rejected bot connection: server has no password set\n");
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player);
          return LOOP_CONTINUE;
        }

        unsigned char nonce[NONCE_SIZE];
        if (send_nonce(new_client, nonce) < 0) {
          fprintf(stderr, "Failed to send nonce\n");
          return -1;
        }

        int auth_ok = (verify_client_password(new_client, password, nonce) == 0 && *password);

        if (is_bot && !auth_ok) {
          printf("Rejected bot connection: authentication failed\n");
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player);
          return LOOP_CONTINUE;
        }

        if (auth_ok) {
          puts("Client authenticated!");
          player->is_admin = true;
        } else {
          player->is_admin = false;
        }

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
        if (recv_all_tcp(new_client, player->nick, len) != (int)len) {
          fprintf(stderr, "Failed to receive nickname.\n");
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player);
          return LOOP_CONTINUE;
        }

        // Step 5: Null terminate
        player->nick[len] = '\0';
        verbose_printf("received nick: %s\n", player->nick);
        ensure_unique_nick(args->game_state, player, slot);
      } else {
        /* In test mode all clients are granted admin so that kick/ban
         * functionality can be exercised from any position in the test suite. */
        slot_id->is_admin = true;
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
  uint32_t autodeal_start = 0;

  uint32_t last_ping_time = SDL_GetTicks();
  uint32_t ping_times[MAX_CLIENTS] = {0};

  /* Ban list lives outside the loop so bans persist across game rounds. */
  Uint32 session_ban_list[64] = {0};
  int session_ban_count = 0;

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
        .ban_count = session_ban_count,
    };
    memcpy(args_broadcast_game_state.ban_list, session_ban_list, sizeof(session_ban_list));

    uint8_t active_clients = count_active_clients(slot_taken);
    int8_t *dealer_id = &game_state.dealer_id;

    ELoop_t ret = register_new_client(&args_broadcast_game_state);
    if (ret == LOOP_CONTINUE)
      continue;
    else if (ret == LOOP_BREAK)
      break;

    if (*dealer_id == -1) {
      for (int8_t i = 0; i < MAX_CLIENTS; i++) {
        if (slot_taken[i]) {
          *dealer_id = i;
          printf("Initial dealer set to player %d\n", i);
          break;
        }
      }
    }

    active_clients = count_active_clients(slot_taken);
    if (active_clients == 0) {
      SDL_Delay(10);
      continue;
    }

    if (active_clients > 0) {
      uint32_t now = SDL_GetTicks();
      bool should_broadcast = false;
      if (now - last_ping_time >= 5000) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (!clients[i])
            continue;
          if (send_ping_request(clients[i]) < 0)
            fprintf(stderr, "[PING] Failed to send ping request to client %d\n", i);
        }
        last_ping_time = now;
        should_broadcast = true;
      }
      int recv_pings = SDLNet_CheckSockets(socket_set, 50);
      if (recv_pings == -1)
        fputs(SDLNet_GetError(), stderr);
      else if (recv_pings != 0) {
        bool break_loop = false;
        for (int8_t i = 0; i < MAX_CLIENTS; i++) {
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

            GameSelectPayload_t payload = {0};
            if (!get_game_select_payload(buffer, size, i, &payload))
              break;
            args_broadcast_game_state.game_type = payload.game_type;
            args_broadcast_game_state.deuces_wild = (payload.deuces_wild != 0);

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
              /* Persist any new bans added during the game. */
              memcpy(session_ban_list, args_broadcast_game_state.ban_list,
                     sizeof(session_ban_list));
              session_ban_count = args_broadcast_game_state.ban_count;
              dealer_timeout_start = 0;
              autodeal_start = 0;
            } else {
              fprintf(stderr, "Non-dealer client %d sent MSG_GAME_SELECT (ignored)\n", i);
            }
            break;
          }

          case MSG_KICK_PLAYER:
          case MSG_BAN_PLAYER: {
            if (!args_broadcast_game_state.game_state->player[i].is_admin)
              break;
            if (size <= OPCODE_SIZE)
              break;
            int8_t target_id = (int8_t)buffer[OPCODE_SIZE];
            if (target_id == i) /* admin can't kick/ban themselves */
              break;
            if (opcode == MSG_KICK_PLAYER)
              kick_player(&args_broadcast_game_state, target_id);
            else
              ban_player(&args_broadcast_game_state, target_id);
            /* Persist any bans added in the lobby. */
            session_ban_count = args_broadcast_game_state.ban_count;
            memcpy(session_ban_list, args_broadcast_game_state.ban_list, sizeof(session_ban_list));
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
      if (should_broadcast)
        broadcast_ping_times(&args_broadcast_game_state, ping_times);
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

    if (cli_args->autodeal) {
      if (active_clients > 1) {
        if (autodeal_start == 0)
          autodeal_start = SDL_GetTicks();
        else if (SDL_GetTicks() - autodeal_start >= 6000) {
          printf("Auto-dealing: no game selected after 6 seconds\n");
          args_broadcast_game_state.game_type =
              game_choices[pcg32_boundedrand_r(&rng, MAX_CHOICES)].game_type;
          args_broadcast_game_state.deuces_wild = false;
          init_game(&args_broadcast_game_state, &deck);
          memcpy(session_ban_list, args_broadcast_game_state.ban_list, sizeof(session_ban_list));
          session_ban_count = args_broadcast_game_state.ban_count;
          dealer_timeout_start = 0;
          autodeal_start = 0;
        }
      } else
        autodeal_start = 0;
    }

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
