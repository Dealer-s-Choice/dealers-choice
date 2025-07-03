/*
 server.c
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

#include <pokeval.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "dc_config.h"
#include "game.h"
#include "server.h"
#include "util.h"

#define MAX_DISCARDS 4

#define SEND_RETRY_COUNT 3
#define SEND_RETRY_DELAY_MS 500

#define handle_round() handle_round_real(args)

typedef struct {
  uint8_t n_winners;
  int id[MAX_PLAYERS];
} RoundResults;

typedef struct {
  uint8_t discard_count;
  uint8_t discard_indices[MAX_DISCARDS];
} DrawRequestMsg_t;

static void remove_disconnected_player(ArgsBroadcastGameState_t *args, const int8_t pl_index);

static bool handle_disconnections(ArgsBroadcastGameState_t *args);

static void init_new_round(GameState_t *game_state);

static uint8_t count_active_clients(const bool *slot_taken);

static void broadcast_start_action_timer_msg(const ArgsBroadcastGameState_t *args);

typedef enum { LOOP_BREAK, LOOP_CONTINUE, LOOP_OK } ELoop_t;
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

ServerConfig_t init_game_state(GameState_t *game_state, Path_t *path, CliArgs_t *cli_args) {
  ServerConfig_t config = get_server_config(path, cli_args);
  for (int i = 0; i < MAX_PLAYERS; i++) {
    game_state->player[i] = (Player_t){
        .id = -1,
        .coins = config.starting_coins,
        .in = false,
        .total_paid = 0,
        .winner = false,
        .has_checked = false,
    };
    snprintf(game_state->player[i].nick, sizeof game_state->player[i].nick, "Player %d", i);
  }

  game_state->dealer_id = 0;
  game_state->at_menu = true;
  game_state->player_count = 0;
  game_state->total_bets_plus_raises = 0;
  game_state->winner_declared = false;
  return config;
}

GameSettings_t init_game_settings(const ServerConfig_t *config, const CliArgs_t *cli_args) {
  GameSettings_t game_settings = {
      .action_timeout_ms = config->action_timeout_ms,
      .end_of_game_timeout_ms = (cli_args->test_mode) ? 500 : config->end_of_game_timeout_ms,
  };
  return game_settings;
}

// In the future, hands will be sent using functions like this, rather than how it's
// presently done in broadcast_game_state()
static int send_new_hand(TCPsocket sock, const POKEVAL_Hand *hand, uint8_t hand_size) {
  if (hand_size == 0 || hand_size > POKEVAL_HAND_SIZE)
    return -1;

  // TODO: Can this be simplified via protobuf-c (already being used to serialize game_state)?

  const size_t card_bytes = hand_size * 8;        // each card is 8 bytes: 2 × 4-byte ints
  const size_t payload_size = 2 + 1 + card_bytes; // opcode + hand_size + cards
  const uint32_t total_size = htonl(payload_size);

  uint8_t buffer[4 + payload_size];
  memcpy(buffer, &total_size, 4); // size prefix

  buffer[4] = (MSG_NEW_HAND >> 8) & 0xFF;
  buffer[5] = MSG_NEW_HAND & 0xFF;
  buffer[6] = hand_size;

  // Serialize each card (face_val and suit as 4-byte integers)
  for (uint8_t i = 0; i < hand_size; ++i) {
    uint32_t fv = htonl(hand->card[i].face_val);
    uint32_t s = htonl(hand->card[i].suit);
    memcpy(&buffer[7 + i * 8], &fv, 4);
    memcpy(&buffer[7 + i * 8 + 4], &s, 4);
  }

  return send_all_tcp(sock, buffer, 4 + payload_size);
}

RealHand_t deal_cards_to_players(GameState_t *game_state, DH_Deck *deck, const uint8_t game_type) {
  RealHand_t real_hand = {0};
  Player_t *players_array = game_state->player;
  Player_t *turn = get_next_player(players_array, game_state->dealer_id);
  Player_t *starting_turn = turn;

  do {
    if (game_type != game_choices[FIVE_CARD_STUD].game_type) {
      for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
        turn->hand.card[i] = DH_card_back;
        real_hand.player[turn->id].card[i] = DH_deal_top_card(deck);
      }
    } else {
      POKEVAL_Hand *hand = &turn->hand;
      // First card face down
      hand->card[0] = DH_card_back;
      real_hand.player[turn->id].card[0] = DH_deal_top_card(deck);

      // Second card face up
      hand->card[1] = DH_deal_top_card(deck);
      real_hand.player[turn->id].card[1] = hand->card[1];

      for (int i = 2; i < POKEVAL_HAND_SIZE; i++) {
        hand->card[i] = DH_card_null;
        real_hand.player[turn->id].card[i] = hand->card[i];
      }
    }
  } while ((turn = get_next_player(players_array, turn->id)) != starting_turn);

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

    POKEVAL_Hand hand_tmp = {0};
    if (args->game_state->winner_declared && args->game_state->player_count != 1) {
      memcpy(&args->game_state->player[i].hand, &args->real_hand->player[i], sizeof(POKEVAL_Hand));

    } else {
      memcpy(&hand_tmp, &args->game_state->player[i].hand, sizeof(POKEVAL_Hand));
      memcpy(&args->game_state->player[i].hand, &args->real_hand->player[i], sizeof(POKEVAL_Hand));
    }

    size_t size = 0;
    uint8_t *data = serialize_game_state(args->game_state, &size);
    if (!data)
      return;

    if (!args->game_state->winner_declared || args->game_state->player_count == 1)
      memcpy(&args->game_state->player[i].hand, &hand_tmp, sizeof(POKEVAL_Hand));

    uint32_t size_net = htonl(size);

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

  uint32_t size_net = htonl(size);

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
  if (msg_len > 100)
    msg_len = 100;

  uint32_t size = htonl(2 + msg_len); // payload: 2-byte opcode + N-byte msg
  uint8_t buffer[4 + 2 + 100];        // max: 4 bytes (size) + 2 (opcode) + 100 (msg)

  memcpy(buffer, &size, 4);

  buffer[4] = (MSG_STATUS_MESSAGE >> 8) & 0xFF;
  buffer[5] = MSG_STATUS_MESSAGE & 0xFF;

  memcpy(&buffer[6], msg, msg_len);

  // Send total (size prefix + payload)
  return send_all_tcp(sock, buffer, 6 + msg_len);
}

static void broadcast_status_message(const ArgsBroadcastGameState_t *args, const char *msg) {
  if (count_active_clients(args->slot_taken) == 0)
    return;
  int8_t pl_idx = args->game_state->turn_id;
  Player_t *recipient = &args->game_state->player[pl_idx];
  if (recipient->id == -1)
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
  } while ((recipient = get_next_connected_client(args->game_state->player, pl_idx)) != start);
}

static int8_t recv_game_select(TCPsocket sock, uint8_t *out_game_type) {
  uint8_t buffer[3];

  int bytes_n;
  if ((bytes_n = recv_all_tcp(sock, buffer, sizeof(buffer))) <= 0)
    return bytes_n;

  uint16_t opcode = (buffer[0] << 8) | buffer[1];
  if (opcode != MSG_GAME_SELECT)
    return -1;

  *out_game_type = buffer[2];
  return bytes_n;
}

static int recv_player_action(TCPsocket sock, PlayerActionMsg_t *out_action) {
  uint8_t buffer[7];

  int n_bytes;
  if ((n_bytes = recv_all_tcp(sock, buffer, sizeof(buffer))) <= 0) {
    fprintf(stderr, "Failed to receive player action\n");
    return n_bytes;
  }

  uint16_t opcode = (buffer[0] << 8) | buffer[1];
  if (opcode != MSG_PLAYER_ACTION)
    return -1;

  out_action->action = buffer[2];
  out_action->amount = ((uint32_t)buffer[3] << 24) | ((uint32_t)buffer[4] << 16) |
                       ((uint32_t)buffer[5] << 8) | ((uint32_t)buffer[6]);

  fprintf(stderr, "Received action %u with amount %u\n", out_action->action, out_action->amount);
  return n_bytes;
}

static int send_draw_prompt(TCPsocket sock) {
  uint8_t buffer[6]; // 4 bytes size + 2 bytes opcode

  uint32_t size = htonl(2); // payload is 2 bytes
  memcpy(buffer, &size, 4);

  buffer[4] = (MSG_DRAW_PROMPT >> 8) & 0xFF;
  buffer[5] = MSG_DRAW_PROMPT & 0xFF;

  int sent = send_all_tcp(sock, buffer, sizeof(buffer));
  printf("Sent draw prompt: %d bytes\n", sent);
  return sent;
}

static int send_start_action_timeout_msg(TCPsocket sock) {
  uint8_t buffer[6]; // 4 bytes size + 2 bytes opcode

  uint32_t size = htonl(2); // payload is 2 bytes
  memcpy(buffer, &size, 4);

  buffer[4] = (MSG_START_ACTION_TIMER >> 8) & 0xFF;
  buffer[5] = MSG_START_ACTION_TIMER & 0xFF;

  int sent = send_all_tcp(sock, buffer, sizeof(buffer));
  return sent;
}

static void broadcast_start_action_timer_msg(const ArgsBroadcastGameState_t *args) {
  int8_t pl_idx = args->game_state->turn_id;
  Player_t *recipient = &args->game_state->player[pl_idx];
  if (recipient->id == -1)
    recipient = get_next_connected_client(args->game_state->player, pl_idx);
  Player_t *start = recipient;

  do {
    pl_idx = recipient->id;
    TCPsocket sock = args->clients[pl_idx];
    if (!sock)
      continue;

    if (send_start_action_timeout_msg(sock) < 0) {
      fprintf(stderr, "[broadcast_start_action_timer_message] Failed to send to client %d\n",
              pl_idx);
    }
  } while ((recipient = get_next_connected_client(args->game_state->player, pl_idx)) != start);
}

static void server_handle_call(GameState_t *game_state, const uint8_t turn_id) {
  uint32_t owed = game_state->total_bets_plus_raises - game_state->player[turn_id].total_paid;
  game_state->player[turn_id].coins -= owed;
  game_state->player[turn_id].total_paid += owed;
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
  } while ((turn = get_next_player(game_state->player, turn->id)) != dealer);
}

static void server_handle_bet(GameState_t *game_state, const uint8_t turn_id,
                              const uint32_t amount) {
  game_state->player[turn_id].coins -= amount;
  game_state->player[turn_id].total_paid += amount;
  game_state->total_bets_plus_raises += amount;
  game_state->pot += amount;
}

// On Ubuntu 24.04 arm64: error: conflicting types for ‘raise’; so I've given
// this a more unique name now
static void server_handle_raise(GameState_t *game_state, const uint8_t turn_id,
                                const uint32_t amount) {
  server_handle_call(game_state, turn_id);
  server_handle_bet(game_state, turn_id, amount);
}

static int handle_draw(ArgsBroadcastGameState_t *args, TCPsocket sock, const int id,
                       DH_Deck *deck) {
  puts("sending draw prompt");
  if (send_draw_prompt(sock) != 0) {
    fputs("Failed to send draw prompt\n", stderr);
    return -1;
  }
  broadcast_start_action_timer_msg(args);

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
      return -1;
    }
    if (num_ready > 0) {
      if (SDLNet_SocketReady(sock)) {
        n_bytes = recv_all_tcp(sock, buffer, sizeof(buffer));
        if (n_bytes > 0)
          break;
        else {
          remove_disconnected_player(args, id);
          return -1;
          break;
        }
      } else {
        handle_disconnections(args);
      }
    }
  }

  if (*buffer != 0) {
    uint16_t opcode = (buffer[0] << 8) | buffer[1];
    if (opcode != MSG_DRAW_REQUEST)
      return -1;
  }

  uint8_t count = buffer[2];
  if (count > MAX_DISCARDS)
    return -1;

  req.discard_count = count;
  memcpy(req.discard_indices, &buffer[3], MAX_DISCARDS); // copy all 4

  printf("Player wants to discard %u cards: ", req.discard_count);
  for (int i = 0; i < req.discard_count; ++i) {
    printf("%u ", req.discard_indices[i]);
    args->real_hand->player[id].card[req.discard_indices[i]] = DH_deal_top_card(deck);

    puts("");
  }

  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof status_str, "%s drew %d", args->game_state->player[id].nick,
           req.discard_count);
  send_new_hand(sock, &args->real_hand->player[id], POKEVAL_HAND_SIZE);
  broadcast_status_message(args, status_str);

  return 0;
}

static EPlayerAction_t handle_check(Player_t *turn, PlayerActionMsg_t *action) {
  turn->has_checked = true;
  action->str = _("checked");
  return ACTION_CHECK;
}

static EPlayerAction_t handle_fold(GameState_t *game_state, Player_t *turn,
                                   PlayerActionMsg_t *action) {
  turn->in = false;
  game_state->player_count--;
  action->str = _("folded");
  return ACTION_FOLD;
}

static bool has_paid_all_bets(const GameState_t *game_state, const Player_t *player) {
  return player->total_paid == game_state->total_bets_plus_raises;
}

static void determine_winner(ArgsBroadcastGameState_t *args, RoundResults *results) {
  if (results->n_winners > 0)
    return;

  // When set to true, the opponents` cards will be revealed to all the players the next
  // time broadcast_game_state is called
  args->game_state->winner_declared = true;

  Player_t *players_array = args->game_state->player;
  Player_t *starting_player = players_array;
  uint8_t pl_count = args->game_state->player_count;

  // I've seen this twice now during testing. Maybe happens during a tie.
  //
  // ../src/server.c:350:39: runtime error: variable length array bound evaluates to non-positive
  // value 0
  // ../subprojects/pokeval/pokeval.c:298:11: runtime error: variable length array bound evaluates
  // to non-positive value 0
  POKEVAL_NeedComparing need_comparing[pl_count];
  Player_t *ptr = starting_player;
  for (uint8_t i = 0; i < pl_count; i++) {
    need_comparing[i].won = false;
    need_comparing[i].id = ptr->id;
    memcpy(&need_comparing[i].hand, &args->real_hand->player[ptr->id], sizeof(POKEVAL_Hand));
    ptr = get_next_player(players_array, ptr->id);
  }

  results->n_winners = POKEVAL_compare_hands(need_comparing, pl_count);
  uint8_t winners = 0;

  // Ties are not fully implemented yet. POKEVAL_compare_hands() handles them, but
  // the tests need to be reviewed and perhaps added to in the pokeval library. The code
  // here to report ties and distribute the pot to tied players isn't complete.
  for (int i = 0; i < pl_count; i++) {
    if (!need_comparing[i].won)
      continue;
    results->id[winners++] = need_comparing[i].id;
    Player_t *winner = &args->game_state->player[need_comparing[i].id];
    winner->winner = true;
    // fprintf(stderr, "winner id: %d\n", need_comparing[i].id);
    char status_str[LEN_STATUS_STR];
    snprintf(status_str, sizeof(status_str),
             // When broadcast is called, it will reveal the cards if winner has been declared. We
             // don't need to call that yet, so using the values from "real_hand" for now
             "%s wins with %s", winner->nick,
             POKEVAL_rank[POKEVAL_evaluate_hand(args->real_hand->player[winner->id])]);
    broadcast_status_message(args, status_str);
    if (args->cli_args->server_log_game_results_file) {
      FILE *fp = fopen(args->cli_args->server_log_game_results_file, "a");
      if (!fp)
        perror("fopen");
      else {
        fprintf(fp, "pot: %d</br>\n", args->game_state->pot);
        fprintf(fp, "%s wins with %s\n\n", winner->nick,
                POKEVAL_rank[POKEVAL_evaluate_hand(args->real_hand->player[winner->id])]);
        fclose(fp);
      }
    }
    uint32_t share = args->game_state->pot / results->n_winners;
    args->game_state->pot = args->game_state->pot % results->n_winners;
    winner->coins += share;
  }
  broadcast_game_state(args);
}

static void award_last_player_in_game(ArgsBroadcastGameState_t *args, Player_t *turn,
                                      RoundResults *results) {
  if (turn->id == -1 || !turn->in) {
    // fprintf(stderr, "turn->id: %d | %d\n", turn->id, __LINE__);
    turn = get_next_player(args->game_state->player, 0);
  }
  turn->winner = true;
  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof(status_str), "%s wins", turn->nick);
  broadcast_status_message(args, status_str);
  if (args->cli_args->server_log_game_results_file) {
    FILE *fp = fopen(args->cli_args->server_log_game_results_file, "a");
    if (!fp)
      perror("fopen");
    else {
      fprintf(fp, "pot: %d</br>\n", args->game_state->pot);
      fprintf(fp, "%s wins\n\n", turn->nick);
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
  // Points to the address of the array of all the players
  Player_t *players_array = args->game_state->player;
  Player_t *starting_player = get_next_player(players_array, args->game_state->dealer_id);
  printf("starting_player id: %d\n", starting_player->id);

  Player_t *turn = starting_player;
  printf("%s:turn->id: %d\n", __func__, turn->id);

  RoundResults results = {0};

  do {
    args->game_state->turn_id = turn->id;
    broadcast_game_state(args);
    broadcast_start_action_timer_msg(args);

    uint32_t wait_ms = args->game_settings->action_timeout_ms;
    uint32_t start = SDL_GetTicks();
    PlayerActionMsg_t action = {0};

    int8_t save_id = turn->id;

    while (SDL_GetTicks() - start < wait_ms) {
      register_new_client(args);
      // fprintf(stderr, "Waiting for action from %d\n", args->game_state->turn_id);
      int n_ready = SDLNet_CheckSockets(args->socket_set, 100); // wait up to 100ms
      if (n_ready > 0) {
        // If this socket is ready (the player who's turn it is), they either
        // disconnected, or have sent an action.
        if (SDLNet_SocketReady(args->clients[turn->id])) {
          // puts("socket ready");
          // char tmp[sizeof args->game_state->status_str];
          if (recv_player_action(args->clients[turn->id], &action) > 0) {
            if (args->game_state->total_bets_plus_raises == 0) {
              switch (action.action) {
              case ACTION_CHECK:
                handle_check(turn, &action);
                break;
              case ACTION_BET:
                server_handle_bet(args->game_state, turn->id, action.amount);
                action.str = _("bet ");
                break;
              case ACTION_FOLD:
                handle_fold(args->game_state, turn, &action);
                break;
              default:
                fprintf(stderr, "Invalid Action received\n");
                exit(EXIT_FAILURE);
              }
            } else {
              switch (action.action) {
              case ACTION_CALL:
                server_handle_call(args->game_state, turn->id);
                action.str = _("called");
                break;
              case ACTION_RAISE:
                server_handle_raise(args->game_state, turn->id, action.amount);
                action.str = _("raised ");
                break;
              case ACTION_FOLD:
                handle_fold(args->game_state, turn, &action);
                break;
              default:
                fprintf(stderr, "Invalid Action received\nThe client is writing checks their body "
                                "can't cash.\n");
                exit(EXIT_FAILURE);
              }
            }
          } else {
            remove_disconnected_player(args, args->game_state->turn_id);
            break;
          }
          break;
        } else {
          // There is data to be read, but not from the player whos turn it is, so probably
          // a disconnect by another client.
          handle_disconnections(args);
          if (args->game_state->player_count == 1)
            break;
        }
      }
      SDL_Delay(50); // avoid busy-waiting
    }

    char status_str[LEN_STATUS_STR] = {0};
    if (args->game_state->player_count > 1) {
      // The id will be -1 if the player disconnected when it was their turn to
      // send an action
      if (turn->id != -1) {
        if (action.action == 0) {
          if (!has_paid_all_bets(args->game_state, turn)) {
            action.action = handle_fold(args->game_state, turn, &action);
          } else if (args->game_state->total_bets_plus_raises == 0) {
            action.action = handle_check(turn, &action);
          }
        }

        if (action.amount > 0)
          snprintf(status_str, sizeof status_str, "%s %s%d", turn->nick, action.str, action.amount);
        else
          snprintf(status_str, sizeof status_str, "%s %s", turn->nick, action.str);
      }

      broadcast_status_message(args, status_str);
      puts(status_str);

      // player_count might be 1 now, if a player folded due to an action timeout
      if (args->game_state->player_count == 1) {
        award_last_player_in_game(args, turn, &results);
        break;
      }

      turn = get_next_player(players_array, save_id);
      if (args->game_state->total_bets_plus_raises == 0) {
        if (turn == starting_player || starting_player->id == -1)
          break;
      } else if (has_paid_all_bets(args->game_state, turn)) {
        break; // Everyone either checked or paid all bets and raises
      }

      if (results.n_winners > 0) {
        break;
      }
    } else {
      snprintf(status_str, sizeof status_str, "%s %s", turn->nick, action.str);
      broadcast_status_message(args, status_str);
      award_last_player_in_game(args, turn, &results);
      break;
    }
  } while (true);

  init_new_round(args->game_state);
  return results;
}

static void reset_players(GameState_t *game_state) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    Player_t *player = &game_state->player[i];
    if (player->id == -1)
      continue;
    player->in = true;
    player->total_paid = 0;
    player->winner = false;
    player->has_checked = false;
    memset(&player->hand, 0, sizeof(player->hand));
  }
}

static void init_new_round(GameState_t *game_state) {
  game_state->total_bets_plus_raises = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (game_state->player[i].id == -1)
      continue;
    game_state->player[i].total_paid = 0;
    game_state->player[i].has_checked = false;
  }
}

static void remove_disconnected_player(ArgsBroadcastGameState_t *args, const int8_t pl_index) {
  Player_t *p = &args->game_state->player[pl_index];
  const int id = p->id;
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
  p->total_paid = 0;
  p->winner = false;
  p->has_checked = false;
  p->in = false;
  p->id = -1;

  // If the disconnect happened during a game, not at the menu
  if (args->game_state->player_count > 0) {
    args->game_state->player_count--;
    char status_str[LEN_STATUS_STR] = {0};
    snprintf(status_str, sizeof status_str, _("%s disconnected"), p->nick);
    broadcast_status_message(args, status_str);
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
  (void)n_stud_new_cards;
  uint8_t dealer_id = args->game_state->dealer_id;
  Player_t *starting_player = get_next_player(players_array, dealer_id);
  server_handle_ante(args->game_state, args->config->ante);

  Player_t *turn = starting_player;
  RoundResults results = {0};
  int8_t save_starting_player_id = starting_player->id;

  for (int i = 0; i < n_betting_rounds; i++) {
    results = handle_round();
    if (results.n_winners > 0 || i == n_draws)
      break;

    if (!starting_player->in) {
      starting_player = get_next_player(players_array, save_starting_player_id);
      save_starting_player_id = starting_player->id;
    }
    turn = starting_player;
    int8_t save_id;
    do {
      args->game_state->turn_id = turn->id;
      save_id = turn->id;

      // The player's new cards are sent to them in handle_draw(), but
      // the clients use game_state.turn_id to decide which buttons to display.
      // It would be better if that behavior were changed so that broadcasting the
      // entire game state wasn't required here (the only info that needs updating
      // at this point is turn_id).
      broadcast_game_state(args);

      if (handle_draw(args, args->clients[turn->id], turn->id, deck) != 0) {
        printf("Failed to receive cards or player disconnected: %d\n", turn->id);
        if (args->game_state->player_count == 1) {
          award_last_player_in_game(args, turn, &results);
          break;
        }
      }

    } while ((turn = get_next_player(players_array, save_id)) != starting_player);
    broadcast_game_state(args);
    if (results.n_winners > 0)
      break;
  }
  determine_winner(args, &results);
}

void game_five_card_stud(GAME_ARGS) {
  (void)n_draws;
  Player_t *starting_player = get_next_player(players_array, args->game_state->dealer_id);
  Player_t *turn = starting_player;
  server_handle_ante(args->game_state, args->config->ante);
  int8_t save_starting_player_id = starting_player->id;

  RoundResults results = {0};
  for (int i = 0; i < n_betting_rounds; i++) {
    results = handle_round();

    if (results.n_winners > 0 || i == n_stud_new_cards)
      break;

    if (!starting_player->in) {
      starting_player = get_next_player(players_array, save_starting_player_id);
      save_starting_player_id = starting_player->id;
    }
    turn = starting_player;

    printf("round: %d\n", i);
    do {
      int id = turn->id;
      POKEVAL_Hand *hand = &turn->hand;
      uint8_t n = i + 2;
      hand->card[n] = DH_deal_top_card(deck);
      args->real_hand->player[id].card[n] = hand->card[n];
      // broadcast_game_state(args);
    } while ((turn = get_next_player(players_array, turn->id)) != starting_player);
    broadcast_game_state(args);
  }
  determine_winner(args, &results);
}

static void play_game(const char game_type, ArgsBroadcastGameState_t *args, DH_Deck *deck) {
  DH_shuffle_deck(deck);
  if (!args->cli_args->test_mode) {
    int cut_point = 16 + pcg32_boundedrand_r(&rng, 21);
    DH_cut_deck(deck, cut_point);
  }

  Player_t *players_array = args->game_state->player;
  *args->real_hand = deal_cards_to_players(args->game_state, deck, game_type);
  args->game_state->winner_declared = false;
  args->game_state->player_count = count_active_clients(args->slot_taken);
  fprintf(stderr, "player count: %d\n", args->game_state->player_count);
  args->game_state->total_bets_plus_raises = 0;
  args->game_state->winner_declared = false;

  broadcast_game_state(args);

  const GameChoice_t *choice = find_game_choice_by_type(game_type);
  char tmp[LEN_STATUS_STR] = {0};
  snprintf(tmp, sizeof(tmp), _("Game: %s"), choice->str);
  broadcast_status_message(args, tmp);
  if (choice && choice->func) {
    // Using function pointers...
    choice->func(args, players_array, deck, choice->n_betting_rounds, choice->n_draws,
                 choice->n_stud_new_cards);
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

static EReturnCode_t receive_game_type_and_run_game(ArgsBroadcastGameState_t *args, DH_Deck *deck) {
  uint8_t game_type = 0;
  int8_t *dealer_id = &args->game_state->dealer_id;
  if (recv_game_select(args->clients[*dealer_id], &game_type) == SIZE_MESSAGE_GAME_SELECT) {
    fprintf(stderr, "Client chose game type: 0x%02x\n", game_type);
  } else {
    fprintf(stderr, "Dealer failed to send valid game type or disconnected.\n");
    remove_disconnected_player(args, *dealer_id);
    broadcast_game_state(args);
    SDL_Delay(10);
    return RC_ERR;
  }

  printf("All %d players are ready. Starting game.\n", count_active_clients(args->slot_taken));
  args->game_state->at_menu = false;

  play_game(game_type, args, deck);

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
    fprintf(stderr, "Dealer rotated to player %d\n", next_dealer);
  } else {
    printf("No valid dealer found after rotation\n");
    *dealer_id = -1;
  }
  // broadcast_game_state(args);
  return RC_OK;
}

static int recv_and_validate_protocol_header(TCPsocket sock) {
  puts("Exchanging protocol information...");
  GameProtocolHeader_t hdr = {0};
  if (recv_all_tcp(sock, &hdr, sizeof(hdr)) <= 0) {
    fprintf(stderr, "Failed to receive protocol header\n");
    return -1;
  }

  if (memcmp(hdr.magic, GAME_PROTOCOL_MAGIC, sizeof(hdr.magic)) != 0) {
    fprintf(stderr, "Protocol magic mismatch: got '%.8s'\n", hdr.magic);
    return -1;
  }

  uint32_t version = ntohl(hdr.version);
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
    p->id = -1;
    p->in = false;
  }
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
      if (args->game_state->at_menu)
        slot_id->in = true;
      else {
        for (int i = 0; i < POKEVAL_HAND_SIZE; i++)
          args->real_hand->player[slot].card[i] = DH_card_null;
        memcpy(&args->game_state->player[slot].hand, &args->real_hand->player[slot],
               sizeof(POKEVAL_Hand));
      }

      if (recv_and_validate_protocol_header(new_client) != 0) {
        do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, NULL);
        return LOOP_CONTINUE;
      }

      if (!args->cli_args->test_mode) {
        Player_t *player = &(args->game_state->player)[slot];
        int32_t net_len;

        // Step 1: Recv the size first (must happen before interpreting it)
        if (recv_all_tcp(new_client, &net_len, sizeof(int32_t)) <= 0) {
          // Handle error: client disconnected or invalid
          fprintf(stderr, "Failed to receive nickname length.\n");
          do_socket_cleanup(new_client, args->socket_set, args->slot_taken, slot, player);
          return LOOP_CONTINUE;
        }

        // Step 2: Now convert
        size_t len = ntohl(net_len);

        // Step 3: Validate length
        if (len == 0 || len >= sizeof(player->nick)) {
          fprintf(stderr, "Invalid nickname length: %zu\n", len);
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
        printf("received nick: %s\n", player->nick);
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

int run_server(CliArgs_t *cli_args, Path_t *path) {
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
  fprintf(stderr, "Resolving host: %s\n", (host) ? host : "NULL");
  if (SDLNet_ResolveHost(&ip, host, atoi(DEFAULT_PORT)) == -1) {
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
    };

    uint8_t active_clients = count_active_clients(slot_taken);
    int8_t *dealer_id = &game_state.dealer_id;
    int num_ready = SDLNet_CheckSockets(socket_set, 0);
    if (num_ready > 0) {
      if (active_clients > 1 && SDLNet_SocketReady(clients[*dealer_id]) &&
          game_state.player[*dealer_id].id != -1) {
        EReturnCode_t status = receive_game_type_and_run_game(&args_broadcast_game_state, &deck);
        dealer_timeout_start = 0;
        if (status == RC_ERR)
          continue;
      } else {
        handle_disconnections(&args_broadcast_game_state);
      }
    }

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

    if (active_clients > 1) {
      if (dealer_timeout_start == 0) {
        dealer_timeout_start = SDL_GetTicks();
      } else if (SDL_GetTicks() - dealer_timeout_start >= config.dealer_timeout_ms) {
        *dealer_id = get_next_dealer(*dealer_id, slot_taken);
        dealer_timeout_start = 0;
        broadcast_game_state(&args_broadcast_game_state);
      }
      if (SDLNet_CheckSockets(socket_set, 50) == -1) {
        fputs(SDLNet_GetError(), stderr);
        continue;
      }
    } else if (active_clients == 1)
      dealer_timeout_start = 0;

    if (reassign_dealer_if_needed(&game_state, slot_taken))
      broadcast_game_state(&args_broadcast_game_state);

    SDL_Delay(50);
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
