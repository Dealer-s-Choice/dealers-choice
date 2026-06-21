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
#include "dc_time.h"
#include "game.h"
#include "globals.h"
#include "lan_discovery.h"
#include "registry.h"
#include "server.h"
#include "server_internal.h"
#include "util.h"

#include <sodium.h>

#define RATE_LIMIT_WINDOW_MS 60000u
#define RATE_LIMIT_CAPACITY 512
typedef struct {
  char ip[TCPME_ADDRSTRLEN];
  uint32_t ticks;
} ConnAttempt_t;

static ConnAttempt_t conn_attempts[RATE_LIMIT_CAPACITY];
static int conn_attempts_count = 0;

/* Same logic as rate_limit_check() below but with `now_ms` injected so the
 * unit test (tests/rate_limit.c) can advance time without sleeping.  Production
 * code calls rate_limit_check() which passes dc_get_ticks(). */
bool dc_rate_limit_check_at(const char *ip_str, uint32_t max_per_minute, uint32_t now_ms) {
  uint32_t now = now_ms;
  int j = 0;
  for (int i = 0; i < conn_attempts_count; i++) {
    if (now - conn_attempts[i].ticks < RATE_LIMIT_WINDOW_MS)
      conn_attempts[j++] = conn_attempts[i];
  }
  conn_attempts_count = j;

  int count = 0;
  for (int i = 0; i < conn_attempts_count; i++) {
    if (strcmp(conn_attempts[i].ip, ip_str) == 0)
      count++;
  }

  if ((uint32_t)count >= max_per_minute)
    return false;

  if (conn_attempts_count < RATE_LIMIT_CAPACITY) {
    strncpy(conn_attempts[conn_attempts_count].ip, ip_str, TCPME_ADDRSTRLEN - 1);
    conn_attempts[conn_attempts_count].ip[TCPME_ADDRSTRLEN - 1] = '\0';
    conn_attempts[conn_attempts_count].ticks = now;
    conn_attempts_count++;
  }

  return true;
}

/* Clear the in-process rate-limit state.  For tests only; production never
 * needs this because each server invocation starts with empty state. */
void dc_rate_limit_reset(void) { conn_attempts_count = 0; }

static bool rate_limit_check(const char *ip_str, uint32_t max_per_minute) {
  return dc_rate_limit_check_at(ip_str, max_per_minute, dc_get_ticks());
}
#define MAX_WILDS 4

#define PING_THRESHOLD 1000

/* Network-health diagnostics (#307), all gated behind --verbose via dc_log.
 * Thresholds are tunable; they flag anomalies, not every message. */
#define SLOW_SEND_WARN_MS 200  /* a send blocking this long => client not draining */
#define PING_SPIKE_WARN_MS 500 /* lobby ping above this => flaky/parked link */

static void print_socket_addr(tcpme_socket_t sock) {
  char buf[TCPME_ADDRSTRLEN];
  if (tcpme_get_local_addr(sock, buf, sizeof(buf)))
    printf("%s\n", buf);
}

/* Write a "## YYYY-MM-DD" day header to the game-results log when the local
 * date has changed since the last header (and once at the first call), so a
 * server that runs past midnight groups entries under the right day. */
void maybe_log_day_header(const char *path) {
  if (!path)
    return;
  static int last_ymd = 0;
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  int ymd = (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;
  if (ymd == last_ymd)
    return;
  FILE *fp = fopen(path, "a");
  if (!fp) {
    perror("fopen");
    return;
  }
  fprintf(fp, "## %04d-%02d-%02d\n\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  fclose(fp);
  last_ymd = ymd;
}

/* Machine-readable showdown log: one JSON object per line. Cards are emitted
 * with numeric face_val (1=Ace low, 11=J, 12=Q, 13=K) and numeric suit, so an
 * external analyzer can independently re-rank the hands and confirm the
 * server's declared winner. Null/back cards are omitted. */
void log_hands_json(const ArgsBroadcastGameState_t *args, const POKEVAL_NeedComparing *cmp,
                    uint8_t pl_count, uint32_t pot, bool by_fold) {
  if (!args->cli_args->server_log_hands_file)
    return;
  FILE *fp = fopen(args->cli_args->server_log_hands_file, "a");
  if (!fp) {
    perror("fopen");
    return;
  }
  const GameChoice_t *choice = find_game_choice_by_type(args->game_type);
  fprintf(fp, "{\"ts\":%lld", (long long)time(NULL));
  fprintf(fp, ",\"game\":\"%s\"", choice ? choice->str : "?");
  fprintf(fp, ",\"game_type\":%u", (unsigned)args->game_type);
  fprintf(fp, ",\"deuces_wild\":%s", args->deuces_wild ? "true" : "false");
  fprintf(fp, ",\"pot\":%u", pot);
  fprintf(fp, ",\"by_fold\":%s", by_fold ? "true" : "false");

  fprintf(fp, ",\"players\":[");
  for (uint8_t i = 0; i < pl_count; i++) {
    int8_t pid = cmp[i].id;
    const Player_t *p = &args->game_state->player[pid];
    if (i)
      fputc(',', fp);
    fprintf(fp, "{\"id\":%d,\"nick\":\"%s\",\"won\":%s,\"cards\":[", pid, p->nick,
            cmp[i].won ? "true" : "false");
    bool first = true;
    for (int c = 0; c < 9; c++) {
      DH_Card card = cmp[i].hand.card[c];
      if (DH_is_card_null(card))
        continue;
      if (!first)
        fputc(',', fp);
      first = false;
      fprintf(fp, "{\"f\":%d,\"s\":%d,\"slot\":%d}", card.face_val, card.suit,
              choice ? (int)choice->card_slot[c] : -1);
    }
    fputc(']', fp);
    if (cmp[i].won) {
      short rank = args->deuces_wild ? POKEVAL_evaluate_hand_wild(cmp[i].hand_5, DH_CARD_TWO)
                                     : POKEVAL_evaluate_hand(cmp[i].hand_5);
      fprintf(fp, ",\"rank\":\"%s\",\"rank_id\":%d,\"best5\":[", POKEVAL_rank[rank], rank);
      for (int c = 0; c < 5; c++) {
        DH_Card card = cmp[i].hand_5.card[c];
        if (c)
          fputc(',', fp);
        fprintf(fp, "{\"f\":%d,\"s\":%d}", card.face_val, card.suit);
      }
      fputc(']', fp);
    }
    fputc('}', fp);
  }
  fputs("]}\n", fp);
  fclose(fp);
}

/* Variant used when only one player remained (everyone else folded).  No
 * showdown comparison happened, so we log just the surviving player's hand
 * for completeness — the analyzer will skip ranking work for these. */
void log_hands_fold_json(const ArgsBroadcastGameState_t *args, const Player_t *winner,
                         uint32_t pot) {
  if (!args->cli_args->server_log_hands_file)
    return;
  FILE *fp = fopen(args->cli_args->server_log_hands_file, "a");
  if (!fp) {
    perror("fopen");
    return;
  }
  const GameChoice_t *choice = find_game_choice_by_type(args->game_type);
  fprintf(fp,
          "{\"ts\":%lld,\"game\":\"%s\",\"game_type\":%u,\"deuces_wild\":%s,\"pot\":%u,"
          "\"by_fold\":true,\"players\":[{\"id\":%d,\"nick\":\"%s\",\"won\":true,\"cards\":[",
          (long long)time(NULL), choice ? choice->str : "?", (unsigned)args->game_type,
          args->deuces_wild ? "true" : "false", pot, winner->id, winner->nick);
  bool first = true;
  for (int c = 0; c < 9; c++) {
    DH_Card card = args->real_hand[winner->id].card[c];
    if (DH_is_card_null(card))
      continue;
    if (!first)
      fputc(',', fp);
    first = false;
    fprintf(fp, "{\"f\":%d,\"s\":%d,\"slot\":%d}", card.face_val, card.suit,
            choice ? (int)choice->card_slot[c] : -1);
  }
  fputs("]}]}\n", fp);
  fclose(fp);
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
  game_state->round_opener_id = -1;
  game_state->at_menu = true;
  game_state->player_count = 0;
  game_state->raises_remaining = 0;
  game_state->prev_bet_amount = 0;
  game_state->winner_declared = false;
  return config;
}

GameSettings_t init_game_settings(const ServerConfig_t *config) {
  GameSettings_t game_settings = {
      .action_timeout_ms = config->action_timeout_ms,
      .end_of_game_timeout_ms = (dc_test_mode) ? 500 : config->end_of_game_timeout_ms,
      .bet_amount_count = config->bet_amount_count,
  };
  memcpy(game_settings.bet_amounts, config->bet_amounts,
         config->bet_amount_count * sizeof(uint32_t));
  return game_settings;
}

// In the future, hands will be sent using functions like this, rather than how it's
// presently done in broadcast_game_state()
int send_new_hand(tcpme_socket_t sock, const POKEVAL_Hand_9 *hand, uint8_t hand_size) {
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
  uint32_t total_size;
  tcpme_put_be32((uint8_t *)&total_size, payload_size);

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

void deal_cards_to_players(GameState_t *game_state, DH_Deck *deck, const uint8_t game_type,
                           POKEVAL_Hand_9 *real_hand) {
  Player_t *players_array = game_state->player;
  Player_t *turn = get_next_player(players_array, game_state->dealer_id);
  Player_t *starting_turn = turn;

  const GameChoice_t *choice = find_game_choice_by_type(game_type);
  if (!choice) {
    fprintf(stderr, "deal_cards_to_players: unknown game_type 0x%02x\n", game_type);
    return;
  }

  for (int i = 0; i < MAX_HAND_SIZE; i++) {
    turn = starting_turn;
    do {
      if (i >= choice->n_cards_initial_deal) {
        real_hand[turn->id].card[i] = DH_card_null;
        turn->hand.card[i] = DH_card_null;
      } else {
        real_hand[turn->id].card[i] = DH_deal_top_card(deck);
        if (choice->card_slot[i] == CARD_SLOT_FACE_UP ||
            choice->card_slot[i] == CARD_SLOT_COMMUNITY)
          turn->hand.card[i] = real_hand[turn->id].card[i];
        else
          turn->hand.card[i] = DH_card_back;
      }
      turn = get_next_player(players_array, turn->id);
    } while (turn && turn != starting_turn);
  }
}

// At showdown all hands are revealed — required for winner highlighting and
// consistent with poker rules. Called once before the per-client send loop so
// every client receives the same fully-revealed game state.
static void reveal_all_hands(GameState_t *game_state, const POKEVAL_Hand_9 *real_hand) {
  for (int j = 0; j < MAX_PLAYERS; j++)
    memcpy(&game_state->player[j].hand, &real_hand[j], sizeof(POKEVAL_Hand_9));
}

void broadcast_game_state(ArgsBroadcastGameState_t *args) {
  if (args->game_state->winner_declared && args->game_state->player_count != 1)
    reveal_all_hands(args->game_state, args->real_hand);

  bool mask_opponents = !args->game_state->winner_declared || args->game_state->player_count == 1;
  bool no_peek = (args->game_type == game_choices[SEVEN_CARD_NO_PEEK].game_type);
  const GameChoice_t *choice = find_game_choice_by_type(args->game_type);

  // Before sending, build the public visible hand for each player and substitute
  // it into game_state. This means real card values for hidden slots are derived
  // exclusively from real_hand here — they are never serialized from game_state —
  // so a modified client cannot read undealt or face-down card values from the wire.
  //
  // For no-peek: only cards that have been flipped (n < no_peek_n_flipped[p]) are visible.
  // For all other games: CARD_SLOT_HOLE cards are always hidden; FACE_UP and COMMUNITY
  //   cards are always visible.
  POKEVAL_Hand_9 saved_hands[MAX_PLAYERS];
  if (mask_opponents && choice) {
    for (int p = 0; p < MAX_PLAYERS; p++) {
      saved_hands[p] = args->game_state->player[p].hand;
      for (int k = 0; k < MAX_HAND_SIZE; k++) {
        bool visible;
        if (no_peek)
          visible = k < args->no_peek_n_flipped[p];
        else
          visible = (choice->card_slot[k] == CARD_SLOT_FACE_UP ||
                     choice->card_slot[k] == CARD_SLOT_COMMUNITY);
        if (visible)
          args->game_state->player[p].hand.card[k] = args->real_hand[p].card[k];
        else if (DH_is_card_null(args->real_hand[p].card[k]))
          args->game_state->player[p].hand.card[k] = DH_card_null;
        else
          args->game_state->player[p].hand.card[k] = DH_card_back;
      }
    }
  }

  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (!tcpme_socket_valid(args->clients[i])) {
      // fprintf(stderr, "skipping %d\n", i);
      continue;
    }

    // Each client additionally receives their own real cards (all slots).
    // No-peek is the exception: players must not see their own unflipped cards,
    // so the masked view built above is used as-is.
    POKEVAL_Hand_9 hand_tmp = {0};
    bool substitute_own_hand = mask_opponents && !no_peek;
    if (substitute_own_hand) {
      memcpy(&hand_tmp, &args->game_state->player[i].hand, sizeof(POKEVAL_Hand_9));
      memcpy(&args->game_state->player[i].hand, &args->real_hand[i], sizeof(POKEVAL_Hand_9));
    }

    uint32_t size = 0;
    uint8_t *data = serialize_game_state(args->game_state, &size);
    if (!data) {
      if (substitute_own_hand)
        memcpy(&args->game_state->player[i].hand, &hand_tmp, sizeof(POKEVAL_Hand_9));
      break;
    }

    if (substitute_own_hand)
      memcpy(&args->game_state->player[i].hand, &hand_tmp, sizeof(POKEVAL_Hand_9));

    uint32_t size_net;
    tcpme_put_be32((uint8_t *)&size_net, (uint32_t)size);

    // fprintf(stderr, "sending to %d\n", i);
    uint32_t send_start = dc_get_ticks();
    if (send_all_tcp(args->clients[i], &size_net, sizeof(size_net)) != 0 ||
        send_all_tcp(args->clients[i], data, size) != 0) {
      fprintf(stderr, "Failed to send game state to client %d\n", i);
      handle_disconnections(args);
    } else {
      uint32_t send_ms = dc_get_ticks() - send_start;
      if (send_ms >= SLOW_SEND_WARN_MS)
        dc_log(DC_LOG_WARN,
               "slow game-state send to client %d: %ums (client not draining its socket?)", i,
               send_ms);
    }
    free(data);
  }

  if (mask_opponents && choice) {
    for (int p = 0; p < MAX_PLAYERS; p++)
      args->game_state->player[p].hand = saved_hands[p];
  }
}

static void send_game_settings(ArgsBroadcastGameState_t *args, tcpme_socket_t sock) {
  size_t size = 0;
  uint8_t *data = serialize_game_settings(args->game_settings, &size);
  if (!data)
    return;

  uint32_t size_net;
  tcpme_put_be32((uint8_t *)&size_net, (uint32_t)size);

  // fprintf(stderr, "sending to %d\n", i);
  if (send_all_tcp(sock, &size_net, sizeof(size_net)) != 0 || send_all_tcp(sock, data, size) != 0) {
    fprintf(stderr, "Failed to send game settings to client\n");
    handle_disconnections(args);
  }
  free(data);
}

int send_status_message(tcpme_socket_t sock, const char *msg) {
  size_t msg_len = strlen(msg);
  if (msg_len > LEN_STATUS_STR)
    msg_len = LEN_STATUS_STR;

  return send_message(sock, MSG_STATUS_MESSAGE, (const uint8_t *)msg, msg_len);
}

/* Send the same framed message to every connected client, walking the seat ring
 * from the current turn. */
static void broadcast_framed(const ArgsBroadcastGameState_t *args, uint16_t opcode,
                             const uint8_t *payload, size_t len) {
  if (count_active_clients(args->slot_taken) == 0)
    return;
  int8_t pl_idx = args->turn_id;
  Player_t *recipient = &args->game_state->player[pl_idx];
  if (!recipient->is_connected)
    recipient = get_next_connected_client(args->game_state->player, pl_idx);
  Player_t *start = recipient;

  do {
    pl_idx = recipient->id;
    tcpme_socket_t sock = args->clients[pl_idx];
    if (tcpme_socket_valid(sock) && send_message(sock, opcode, payload, len) < 0)
      fprintf(stderr, "[broadcast_framed] Failed to send opcode 0x%04X to client %d\n", opcode,
              pl_idx);

    recipient = get_next_connected_client(args->game_state->player, pl_idx);
  } while (recipient && recipient != start);
}

void broadcast_action_announce(const ArgsBroadcastGameState_t *args, int8_t player_id, int verb,
                              uint32_t amount) {
  ActionAnnounce msg = ACTION_ANNOUNCE__INIT;
  msg.player_id = player_id;
  msg.verb = verb;
  msg.amount = amount;

  uint8_t buf[32];
  size_t len = action_announce__get_packed_size(&msg);
  action_announce__pack(&msg, buf);

  broadcast_framed(args, MSG_ACTION_ANNOUNCE, buf, len);
}

void broadcast_status_message(const ArgsBroadcastGameState_t *args, const char *msg) {
  size_t len = strlen(msg);
  if (len > LEN_STATUS_STR)
    len = LEN_STATUS_STR;
  broadcast_framed(args, MSG_STATUS_MESSAGE, (const uint8_t *)msg, len);
}

void broadcast_game_type(const ArgsBroadcastGameState_t *args) {
  GameSelectPayload_t payload = {args->game_type, args->deuces_wild ? (uint8_t)1 : (uint8_t)0};
  broadcast_framed(args, MSG_GAME_SELECT, (const uint8_t *)&payload, sizeof(payload));
}

static int send_turn_id(tcpme_socket_t sock, const int8_t turn_id) {
  uint8_t payload = (uint8_t)turn_id;
  return send_message(sock, MSG_TURN_ID, &payload, sizeof(payload));
}

void broadcast_turn_id(const ArgsBroadcastGameState_t *args) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!tcpme_socket_valid(args->clients[i]))
      continue;

    tcpme_socket_t sock = args->clients[i];
    if (send_turn_id(sock, args->turn_id) < 0) {
      fprintf(stderr, "[broadcast_turn_id] Failed to send to client %d\n", i);
    }
  }
}

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
ETurnMsg_t recv_turn_player_msg(tcpme_socket_t sock, PlayerActionMsg_t *out_action,
                                uint16_t *out_kb_opcode, int8_t *out_target_id) {
  /* All client messages now use the standard length-prefix framing:
   *   [size:4 BE][opcode:2 BE][payload...] where size = 2 + len(payload). */
  for (;;) {
    uint32_t size_net = 0;
    if (recv_all_tcp(sock, &size_net, sizeof(size_net)) <= 0)
      return TURN_MSG_DISCONNECT;

    uint32_t size = tcpme_get_be32((const uint8_t *)&size_net);
    if (size < 2 || size > 16) {
      fprintf(stderr, "[recv_turn_player_msg] Invalid message size: %u\n", size);
      return TURN_MSG_DISCONNECT;
    }

    uint8_t buf[16];
    if (recv_all_tcp(sock, buf, size) <= 0)
      return TURN_MSG_DISCONNECT;

    uint16_t opcode_be;
    memcpy(&opcode_be, buf, sizeof(opcode_be));
    uint16_t opcode = tcpme_get_be16((const uint8_t *)&opcode_be);

    if (opcode == MSG_PING_RESPONSE)
      continue;

    if (opcode == MSG_PLAYER_ACTION) {
      if (size < 7)
        return TURN_MSG_DISCONNECT;
      out_action->action = buf[2];
      out_action->amount = ((uint32_t)buf[3] << 24) | ((uint32_t)buf[4] << 16) |
                           ((uint32_t)buf[5] << 8) | (uint32_t)buf[6];
      verbose_printf("Received action %u with amount %" PRIu32 "\n", out_action->action,
                     out_action->amount);
      return TURN_MSG_ACTION;
    }

    if (opcode == MSG_KICK_PLAYER || opcode == MSG_BAN_PLAYER) {
      if (size < 3)
        return TURN_MSG_DISCONNECT;
      *out_kb_opcode = opcode;
      *out_target_id = (int8_t)buf[2];
      return TURN_MSG_KICK_BAN;
    }

    fprintf(stderr, "[recv_turn_player_msg] Unrecognised opcode 0x%04X\n", opcode);
    return TURN_MSG_DISCONNECT;
  }
}

int send_opcode(tcpme_socket_t sock, const uint16_t opcode) {
  return send_message(sock, opcode, NULL, 0);
}

static int send_ping_request(tcpme_socket_t sock) {
  PingRequest req = PING_REQUEST__INIT;
  req.timestamp = dc_get_ticks(); // current server tick

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
    if (!tcpme_socket_valid(args->clients[j]))
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
    if (!tcpme_socket_valid(args->clients[i]))
      continue;
    int result = send_message(args->clients[i], MSG_PING_BROADCAST, buf, len);
    if (result < 0) {
      fprintf(stderr, "[PING] Failed to broadcast to client %d\n", i);
    }
  }
  free(buf);
  return 0;
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

void remove_disconnected_player(ArgsBroadcastGameState_t *args, const int8_t id) {
  Player_t *p = &args->game_state->player[id];
  if (tcpme_del_socket(args->socket_set, args->clients[id]) == -1) {
    fputs(tcpme_get_error(), stderr);
    return;
  }

  printf("Client %d disconnected\n", id);
  tcpme_close(args->clients[id]);
  args->clients[id] = TCPME_INVALID_SOCKET;
  args->slot_taken[id] = false;

  /* Whether this player was actually in the current hand. player_count tracks
   * in-hand players (set at hand start, decremented by handle_fold). A mid-hand
   * joiner spectates with in==false and was never counted, and a folded player
   * was already decremented — decrementing again for either would falsely drop
   * player_count and end the hand for everyone else (observer-quit regression). */
  const bool was_in = p->in;

  // Reset player info
  p->coins = args->config->starting_coins;
  p->winner = false;
  p->in = false;
  p->is_connected = false;

  // If the disconnect happened during a game, not at the menu
  if (args->game_state->player_count > 0) {
    char status_str[LEN_STATUS_STR] = {0};
    snprintf(status_str, sizeof status_str, _("%s disconnected"), p->nick);
    broadcast_status_message(args, status_str);

    if (was_in) {
      args->game_state->player_count--;

      /* Only rotate the turn pointer if play_game is still active — between
       * hands `args->starting_turn` is NULL because its backing storage
       * lives on play_game's (returned) stack frame.  Without this guard
       * we'd dereference dead stack memory (caught by ASan as
       * stack-use-after-return). */
      if (args->starting_turn && *args->starting_turn && args->game_state->player_count > 1)
        if ((*args->starting_turn)->id == id)
          *args->starting_turn = get_next_player(args->game_state->player, id);
    }
  }

  memset(p->nick, 0, sizeof(p->nick));
  broadcast_game_state(args);
}

void kick_player(ArgsBroadcastGameState_t *args, int8_t id) {
  if (id < 0 || id >= MAX_CLIENTS || !args->slot_taken[id])
    return;
  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof status_str, _("%s was kicked"), args->game_state->player[id].nick);
  remove_disconnected_player(args, id);
  broadcast_status_message(args, status_str);
  broadcast_game_state(args);
}

void ban_player(ArgsBroadcastGameState_t *args, int8_t id) {
  if (id < 0 || id >= MAX_CLIENTS || !args->slot_taken[id])
    return;
  if (args->ban_count < (int)(sizeof(args->ban_list) / sizeof(args->ban_list[0]))) {
    char ip_str[TCPME_ADDRSTRLEN];
    if (tcpme_get_peer_ip(args->clients[id], ip_str, sizeof(ip_str))) {
      strncpy(args->ban_list[args->ban_count], ip_str, TCPME_ADDRSTRLEN - 1);
      args->ban_list[args->ban_count][TCPME_ADDRSTRLEN - 1] = '\0';
      args->ban_count++;
      printf("Banned IP: %s\n", ip_str);
    }
  }
  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof status_str, _("%s was banned"), args->game_state->player[id].nick);
  remove_disconnected_player(args, id);
  broadcast_status_message(args, status_str);
  broadcast_game_state(args);
}

bool handle_disconnections(ArgsBroadcastGameState_t *args) {
  bool someone_disconnected = false;
  for (int8_t i = 0; i < MAX_CLIENTS; i++) {
    if (!args->slot_taken[i])
      continue;
    if (!tcpme_socket_ready(args->socket_set, args->clients[i]))
      continue;

    /* Read the length prefix to determine if this is a disconnect or a message. */
    uint32_t len_be;
    int r = tcpme_recv(args->clients[i], &len_be, sizeof(len_be));
    if (r <= 0) {
      remove_disconnected_player(args, i);
      someone_disconnected = true;
      continue;
    }

    uint32_t msg_len = tcpme_get_be32((const uint8_t *)&len_be);
    if (msg_len < OPCODE_SIZE || msg_len > 256) {
      remove_disconnected_player(args, i);
      someone_disconnected = true;
      continue;
    }

    uint16_t opcode_be;
    r = tcpme_recv(args->clients[i], &opcode_be, sizeof(opcode_be));
    if (r <= 0) {
      remove_disconnected_player(args, i);
      someone_disconnected = true;
      continue;
    }
    uint16_t opcode = tcpme_get_be16((const uint8_t *)&opcode_be);

    uint32_t payload_len = msg_len - OPCODE_SIZE;
    uint8_t payload[32] = {0};
    if (payload_len > 0) {
      r = tcpme_recv(args->clients[i], payload,
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

uint8_t count_active_clients(const bool *slot_taken) {
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
      memcpy(player->nick, candidate, sizeof(player->nick));
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

  /* play_game stores the turn-rotation pointer in `args->starting_turn`,
   * but the underlying Player_t* lives on play_game's stack frame.  By the
   * time we return here the frame is gone, so any subsequent
   * handle_disconnections call below (which can reach into
   * remove_disconnected_player and dereference args->starting_turn) would
   * read freed stack memory.  ASan catches it as stack-use-after-return.
   * Clear the pointer so the dereference guard in remove_disconnected_player
   * skips the rotation logic that's not meaningful between hands anyway. */
  args->starting_turn = NULL;

  broadcast_game_state(args);

  uint32_t wait_ms = args->game_settings->end_of_game_timeout_ms;
  // uint32_t wait_ms = 2000;
  uint32_t start = dc_get_ticks();
  while (dc_get_ticks() - start < wait_ms) {
    register_new_client(args);
    tcpme_check_sockets(args->socket_set, 10);
    handle_disconnections(args);
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

static int recv_and_validate_protocol_header(tcpme_socket_t sock, uint8_t *flags_out) {
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

  uint32_t version = tcpme_get_be16((const uint8_t *)&hdr.version);
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

static void do_socket_cleanup(tcpme_socket_t sock, tcpme_set_t *socket_set, bool *slot_taken,
                              const int slot, Player_t *p, tcpme_socket_t *client_ref) {
  tcpme_del_socket(socket_set, sock);
  tcpme_close(sock);
  if (client_ref)
    *client_ref = TCPME_INVALID_SOCKET;
  slot_taken[slot] = false;
  if (p) {
    p->is_connected = false;
    p->in = false;
  }
}

static void flush_client_socket(tcpme_socket_t sock) {
  char buffer[512];
  int len;
  tcpme_set_t *tmp_set = tcpme_alloc_set(1);
  if (!tmp_set)
    return;
  tcpme_add_socket(tmp_set, sock);

  for (;;) {
    if (tcpme_check_sockets(tmp_set, 0) <= 0)
      break;
    if (!tcpme_socket_ready(tmp_set, sock))
      break;
    len = tcpme_recv(sock, buffer, sizeof(buffer));
    if (len <= 0)
      break;
  }
  tcpme_free_set(tmp_set);
}

static int send_nonce(tcpme_socket_t sock, unsigned char nonce[NONCE_SIZE]) {
  randombytes_buf(nonce, NONCE_SIZE);
  return send_all_tcp(sock, nonce, NONCE_SIZE);
}

static int verify_client_password(tcpme_socket_t sock, const char *stored_password,
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

/* Answer one pending LAN discovery query, if any (non-blocking). Called from
 * the lobby (in_progress=false) and between actions during a hand
 * (in_progress=true) so the game stays findable throughout. */
static void service_lan_discovery(ArgsBroadcastGameState_t *args, bool in_progress) {
  if (!args->lan_discovery_set)
    return;
  if (tcpme_check_sockets(args->lan_discovery_set, 0) <= 0)
    return;

  /* Both family sockets share one set; build the advertised info once and answer
   * on whichever has a pending query (IPv4 broadcast and/or IPv6 multicast). */
  LanGameInfo_t info = {0};
  info.tcp_port = args->lan_port;
  info.player_count = count_active_clients(args->slot_taken);
  info.max_players = MAX_CLIENTS;
  info.password_protected = (args->config->password[0] != '\0');
  info.in_progress = in_progress;
  snprintf(info.name, sizeof info.name, "%.*s", (int)(sizeof info.name - 1),
           args->config->server_name);

  if (tcpme_socket_valid(args->lan_discovery_sock) &&
      tcpme_socket_ready(args->lan_discovery_set, args->lan_discovery_sock))
    lan_discovery_answer(args->lan_discovery_sock, &info);
  if (tcpme_socket_valid(args->lan_discovery_sock6) &&
      tcpme_socket_ready(args->lan_discovery_set, args->lan_discovery_sock6))
    lan_discovery_answer6(args->lan_discovery_sock6, &info);
}

/* Registry publish policy (DC-owned; tcpme stays generic). */
#define REG_PUB_HEARTBEAT_MS 30000u     /* normal heartbeat interval */
#define REG_PUB_BACKOFF_MID_MS 120000u  /* after 3 consecutive failures */
#define REG_PUB_BACKOFF_LONG_MS 300000u /* after 6 consecutive failures */
#define REG_PUB_CONNECT_MS 300u         /* bounded connect — won't stall the game */
#define REG_PUB_IO_MS 2000u

/* Announce/heartbeat this server to every configured registry. Non-blocking
 * (bounded connect) and rate-limited with exponential backoff (30s normal;
 * 2 min after 3 fails; 5 min after 6) so a down/slow registry never stalls
 * gameplay (#33). */
static void service_registry_publish(ArgsBroadcastGameState_t *args) {
  if (args->cli_args->disable_publish || args->config->registry_count == 0)
    return;
  static uint32_t last_attempt = 0;
  static uint32_t interval = 0; /* 0 => attempt on the first call */
  static int fail_count = 0;
  uint32_t now = dc_get_ticks();
  if (interval != 0 && now - last_attempt < interval)
    return;
  last_attempt = now;

  RegistryServer_t info = {0};
  info.tcp_port = args->lan_port;
  info.player_count = count_active_clients(args->slot_taken);
  info.max_players = MAX_CLIENTS;
  info.password_protected = (args->config->password[0] != '\0');
  info.in_progress = !args->game_state->at_menu;
  snprintf(info.name, sizeof info.name, "%.*s", (int)(sizeof info.name - 1),
           args->config->server_name);

  bool any_ok = false;
  for (int i = 0; i < args->config->registry_count; i++) {
    tcpme_socket_t s = tcpme_connect_timeout(args->config->registry_host[i],
                                             args->config->registry_port[i], REG_PUB_CONNECT_MS);
    if (!tcpme_socket_valid(s))
      continue;
    tcpme_set_timeout(s, REG_PUB_IO_MS);
    if (registry_send_announce(s, &info) == 0)
      any_ok = true;
    tcpme_close(s);
  }

  if (any_ok) {
    fail_count = 0;
    interval = REG_PUB_HEARTBEAT_MS;
  } else {
    fail_count++;
    interval = (fail_count >= 6)   ? REG_PUB_BACKOFF_LONG_MS
               : (fail_count >= 3) ? REG_PUB_BACKOFF_MID_MS
                                   : REG_PUB_HEARTBEAT_MS;
  }
}

ELoop_t register_new_client(ArgsBroadcastGameState_t *args) {
  service_lan_discovery(args, true);
  service_registry_publish(args);
  // checks for and accepts incoming connections
  tcpme_socket_t new_client = tcpme_accept(*args->server_sock);
  if (tcpme_socket_valid(new_client)) {
    tcpme_set_timeout(new_client, SOCKET_IO_TIMEOUT_MS);
    char peer_ip_str[TCPME_ADDRSTRLEN] = "";
    tcpme_get_peer_ip(new_client, peer_ip_str, sizeof(peer_ip_str));

    for (int b = 0; b < args->ban_count; b++) {
      if (strcmp(args->ban_list[b], peer_ip_str) == 0) {
        printf("Rejected banned client\n");
        tcpme_close(new_client);
        return LOOP_CONTINUE;
      }
    }

    // Loopback addresses are exempt from rate/connection limits.
    bool is_loopback = (strcmp(peer_ip_str, "127.0.0.1") == 0 || strcmp(peer_ip_str, "::1") == 0);
    if (!is_loopback) {
      if (args->config->max_connections_per_minute > 0) {
        if (!rate_limit_check(peer_ip_str, args->config->max_connections_per_minute)) {
          printf("Rejected connection: rate limit exceeded\n");
          tcpme_close(new_client);
          return LOOP_CONTINUE;
        }
      }
      if (args->config->max_connections_per_ip > 0) {
        uint32_t count = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (!args->slot_taken[i] || !tcpme_socket_valid(args->clients[i]))
            continue;
          char existing_ip[TCPME_ADDRSTRLEN] = "";
          tcpme_get_peer_ip(args->clients[i], existing_ip, sizeof(existing_ip));
          if (strcmp(existing_ip, peer_ip_str) == 0)
            count++;
        }
        if (count >= args->config->max_connections_per_ip) {
          printf("Rejected connection: max_connections_per_ip reached\n");
          tcpme_close(new_client);
          return LOOP_CONTINUE;
        }
      }
    }

    /* Validate the protocol header BEFORE allocating a slot. A registry verify
     * probe (PROTO_FLAG_PROBE) then completes the handshake and disconnects
     * without taking a slot or logging connect/nickname noise — so it succeeds
     * even when the server is full (#33). */
    uint8_t proto_flags = 0;
    if (recv_and_validate_protocol_header(new_client, &proto_flags) != 0) {
      tcpme_close(new_client);
      return LOOP_CONTINUE;
    }
    if (proto_flags & PROTO_FLAG_PROBE) {
      tcpme_close(new_client);
      return LOOP_CONTINUE;
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
      tcpme_add_socket(args->socket_set, new_client);

      char peer_addr_str[TCPME_ADDRSTRLEN];
      if (tcpme_get_peer_addr(new_client, peer_addr_str, sizeof(peer_addr_str)))
        printf("Client %d connected from %s\n", slot, peer_addr_str);

      Player_t *slot_id = &(args->game_state->player)[slot];
      slot_id->id = slot;
      slot_id->is_connected = true;
      args->player_timeouts[slot] = 0;
      if (args->game_state->at_menu)
        slot_id->in = true;
      else {
        for (int i = 0; i < MAX_HAND_SIZE; i++)
          args->real_hand[slot].card[i] = DH_card_null;
        memcpy(&args->game_state->player[slot].hand, &args->real_hand[slot],
               sizeof(POKEVAL_Hand_9));
      }

      if (!dc_test_mode) {
        Player_t *player = &(args->game_state->player)[slot];

        bool is_bot = (proto_flags & PROTO_FLAG_BOT) != 0;
        const char *password = args->config->password;

        if (is_bot && !*password) {
          printf("Rejected bot connection: server has no password set\n");
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player,
                            &args->clients[slot]);
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
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player,
                            &args->clients[slot]);
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
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player,
                            &args->clients[slot]);
          return LOOP_CONTINUE;
        }

        // Step 2: Now convert
        uint16_t len = tcpme_get_be16((const uint8_t *)&net_len);

        // Step 3: Validate length
        if (len == 0) {
          fprintf(stderr, "Invalid nickname length: %d\n", len);
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player,
                            &args->clients[slot]);
          return LOOP_CONTINUE;
        }
        if (len >= sizeof(player->nick))
          len = (uint16_t)(sizeof(player->nick) - 1);

        // Step 4: Read nickname
        memset(player->nick, 0, sizeof(player->nick));
        if (recv_all_tcp(new_client, player->nick, len) != (int)len) {
          fprintf(stderr, "Failed to receive nickname.\n");
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player,
                            &args->clients[slot]);
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

      /* A client joining a running game missed the MSG_GAME_SELECT that was
       * broadcast at game-select time, so its game-name / deuces-wild
       * indicators stay blank. Send the current selection to just this
       * client; its existing MSG_GAME_SELECT handler populates both (#223). */
      if (!args->game_state->at_menu) {
        if (send_game_select(new_client, args->game_type, args->deuces_wild) < 0)
          fprintf(stderr, "[register_new_client] Failed to send game type to client %d\n", slot);
      }
    } else {
      printf("Server full. Rejecting connection.\n");
      tcpme_close(new_client);
    }
  }
  return LOOP_OK;
}

int run_server(const CliArgs_t *cli_args, Path_t *path) {
  GameState_t game_state = {0};
  ServerConfig_t config = init_game_state(&game_state, path, cli_args);
  if (cli_args->server_name)
    snprintf(config.server_name, sizeof(config.server_name), "%s", cli_args->server_name);
  GameSettings_t game_settings = init_game_settings(&config);
  game_state.pot = 0;

  if (tcpme_init() != 0) {
    fprintf(stderr, "tcpme init failed: %s\n", tcpme_get_error());
    return 1;
  }

  char *host = config.bind_address;
  if (!cli_args->bind_address) {
    host = config.bind_address;
    if (strcmp(config.bind_address, "NULL") == 0)
      host = NULL;
  } else
    host = (char *)cli_args->bind_address;
  verbose_printf("Binding to: %s\n", (host) ? host : "(all interfaces)");
  uint16_t port = (cli_args->port != 0) ? cli_args->port : config.port;

  tcpme_socket_t server = tcpme_listen(host, port);
  if (!tcpme_socket_valid(server)) {
    fprintf(stderr, "tcpme_listen: %s\n", tcpme_get_error());
    tcpme_quit();
    return 1;
  }

  printf("Server listening on ");
  print_socket_addr(server);

  /* LAN discovery: answer queries so clients on the same network can find this
   * game without a registry server. Two responders share one select set: IPv4
   * (limited broadcast) and IPv6 (link-local multicast). Each is optional — if a
   * family's socket can't be opened (port in use, no IPv6), that family is simply
   * disabled and the game still runs. */
  tcpme_socket_t discovery_sock = lan_discovery_open_responder(config.lan_discovery_port);
  tcpme_socket_t discovery_sock6 = lan_discovery_open_responder6(config.lan_discovery_port);
  tcpme_set_t *discovery_set = NULL;
  if (tcpme_socket_valid(discovery_sock) || tcpme_socket_valid(discovery_sock6)) {
    discovery_set = tcpme_alloc_set(2);
    if (discovery_set) {
      if (tcpme_socket_valid(discovery_sock))
        tcpme_add_socket(discovery_set, discovery_sock);
      if (tcpme_socket_valid(discovery_sock6))
        tcpme_add_socket(discovery_set, discovery_sock6);
    }
  } else {
    fprintf(stderr, "LAN discovery disabled: %s\n", tcpme_get_error());
  }

  tcpme_socket_t clients[MAX_CLIENTS];
  for (int i = 0; i < MAX_CLIENTS; i++)
    clients[i] = TCPME_INVALID_SOCKET;
  tcpme_set_t *socket_set = tcpme_alloc_set(MAX_CLIENTS + 1);
  if (!socket_set) {
    fprintf(stderr, "Failed to allocate socket set: %s\n", tcpme_get_error());
    tcpme_close(server);
    tcpme_quit();
    return 1;
  }

  DH_Deck deck = DH_get_new_deck();

  if (!dc_test_mode)
    DH_pcg_srand_auto();
  else
    DH_pcg_srand(1, 1);

  maybe_log_day_header(cli_args->server_log_game_results_file);

  int game_started = 0;
  bool slot_taken[MAX_CLIENTS] = {false};
  uint32_t dealer_timeout_start = 0;
  uint32_t autodeal_start = 0;

  uint32_t last_ping_time = dc_get_ticks();
  uint32_t ping_times[MAX_CLIENTS] = {0};

  /* Ban list lives outside the loop so bans persist across game rounds. */
  char session_ban_list[64][TCPME_ADDRSTRLEN] = {{0}};
  int session_ban_count = 0;

  /* Action-timeout counts live outside the loop so they accumulate across
   * hands: a player who keeps timing out in different hands still reaches the
   * disconnect threshold. A real action resets that player's count to 0. */
  uint8_t session_player_timeouts[MAX_PLAYERS] = {0};

  while (!game_started) {
    ArgsBroadcastGameState_t args_broadcast_game_state = {
        .clients = clients,
        .socket_set = socket_set,
        .game_state = &game_state,
        .slot_taken = slot_taken,
        .cli_args = cli_args,
        .server_sock = &server,
        .game_settings = &game_settings,
        .config = &config,
        .game_type = 0,
        .starting_turn = NULL,
        .turn_id = 0,
        .ban_count = session_ban_count,
        .lan_discovery_sock = discovery_sock,
        .lan_discovery_sock6 = discovery_sock6,
        .lan_discovery_set = discovery_set,
        .lan_port = port,
    };
    memcpy(args_broadcast_game_state.ban_list, session_ban_list, sizeof(session_ban_list));
    memcpy(args_broadcast_game_state.player_timeouts, session_player_timeouts,
           sizeof(session_player_timeouts));

    /* Answer any pending LAN discovery query (non-blocking), so the game is
     * findable even while the lobby is still empty. */
    service_lan_discovery(&args_broadcast_game_state, false);

    uint8_t active_clients = count_active_clients(slot_taken);
    int8_t *dealer_id = &game_state.dealer_id;

    ELoop_t ret = register_new_client(&args_broadcast_game_state);
    memcpy(session_player_timeouts, args_broadcast_game_state.player_timeouts,
           sizeof(session_player_timeouts));
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
      dc_sleep_ms(10);
      continue;
    }

    if (active_clients > 0) {
      uint32_t now = dc_get_ticks();
      bool should_broadcast = false;
      if (now - last_ping_time >= 5000) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (!tcpme_socket_valid(clients[i]))
            continue;
          if (send_ping_request(clients[i]) < 0)
            fprintf(stderr, "[PING] Failed to send ping request to client %d\n", i);
        }
        last_ping_time = now;
        should_broadcast = true;
      }
      int recv_pings = tcpme_check_sockets(socket_set, 50);
      if (recv_pings == -1)
        fputs(tcpme_get_error(), stderr);
      else if (recv_pings != 0) {
        bool break_loop = false;
        for (int8_t i = 0; i < MAX_CLIENTS; i++) {
          if (!tcpme_socket_valid(clients[i]))
            continue;

          if (!tcpme_socket_ready(socket_set, clients[i]))
            continue;

          // Read the message size first (4 bytes)
          uint32_t size_net = 0;
          if (recv_all_tcp(clients[i], &size_net, sizeof(size_net)) <= 0) {
            fprintf(stderr, "[NET] Disconnection while reading size from client %d\n", i);
            remove_disconnected_player(&args_broadcast_game_state, i);
            continue;
          }

          uint32_t size = tcpme_get_be32((const uint8_t *)&size_net);
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
          uint16_t opcode = tcpme_get_be16((const uint8_t *)&opcode_be);
          switch (opcode) {
          case MSG_PING_RESPONSE: {
            PingResponse *resp = ping_response__unpack(NULL, size - 2, buffer + 2);
            if (!resp) {
              fprintf(stderr, "[PING] Failed to unpack PingResponse from client %d\n", i);
            } else {
              now = dc_get_ticks();
              ping_times[i] = now - resp->timestamp;
              if (ping_times[i] >= PING_SPIKE_WARN_MS)
                dc_log(DC_LOG_WARN, "high ping from client %d: %ums", i, ping_times[i]);
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
              if (!dc_test_mode) {
                int ping_discards;
                while ((ping_discards = tcpme_check_sockets(socket_set, PING_THRESHOLD)) != 0) {
                  if (ping_discards == -1) {
                    fputs(tcpme_get_error(), stderr);
                    break;
                  }

                  for (int d = 0; d < MAX_CLIENTS; d++) {
                    if (!tcpme_socket_valid(clients[d]))
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
        dealer_timeout_start = dc_get_ticks();
      } else if (dc_get_ticks() - dealer_timeout_start >= config.dealer_timeout_ms) {
        *dealer_id = get_next_dealer(*dealer_id, slot_taken);
        dealer_timeout_start = 0;
        broadcast_game_state(&args_broadcast_game_state);
      }
    } else if (active_clients == 1)
      dealer_timeout_start = 0;

    if (cli_args->autodeal) {
      if (active_clients > 1) {
        if (autodeal_start == 0)
          autodeal_start = dc_get_ticks();
        else if (dc_get_ticks() - autodeal_start >= 6000) {
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

    memcpy(session_player_timeouts, args_broadcast_game_state.player_timeouts,
           sizeof(session_player_timeouts));
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (slot_taken[i]) {
      tcpme_close(clients[i]);
    }
  }

  if (discovery_set)
    tcpme_free_set(discovery_set);
  if (tcpme_socket_valid(discovery_sock))
    tcpme_close(discovery_sock);
  if (tcpme_socket_valid(discovery_sock6))
    tcpme_close(discovery_sock6);

  tcpme_close(server);
  tcpme_free_set(socket_set);
  tcpme_quit();

  return 0;
}
