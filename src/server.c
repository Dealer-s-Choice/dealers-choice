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

static void remove_disconnected_player(TCPsocket *clients, SDLNet_SocketSet socket_set,
                                       bool *slot_taken, Player_t *p);

static bool handle_disconnections(TCPsocket *clients, SDLNet_SocketSet socket_set, bool *slot_taken,
                                  GameState_t *game_state);

static void init_new_round(GameState_t *game_state);

static uint8_t count_active_clients(const bool *slot_taken);

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

Config_t init_game_state(GameState_t *game_state, Path_t *path, const bool test_mode) {
  Config_t config = get_config(path);
  for (int i = 0; i < MAX_PLAYERS; i++) {
    game_state->player[i] = (Player_t){
        .id = -1,
        .coins = STARTING_N_COINS,
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
  game_state->action_time_out_ms = config.action_time_out_ms;
  game_state->end_of_round_time_out_ms = (test_mode) ? 500 : config.end_of_round_time_out_ms;
  return config;
}

// In the future, hands will be sent using functions like this, rather than how it's
// presently done in broadcast_game_state()
static int send_new_hand(TCPsocket sock, const struct pokeval_hand_t *hand, uint8_t hand_size) {
  if (hand_size == 0 || hand_size > HAND_SIZE)
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
      for (int i = 0; i < HAND_SIZE; ++i) {
        turn->hand.card[i] = DH_card_back;
        real_hand.player[turn->id].card[i] = DH_deal_top_card(deck);
      }
    } else {
      struct pokeval_hand_t *hand = &turn->hand;
      // First card face down
      hand->card[0] = DH_card_back;
      real_hand.player[turn->id].card[0] = DH_deal_top_card(deck);

      // Second card face up
      hand->card[1] = DH_deal_top_card(deck);
      real_hand.player[turn->id].card[1] = hand->card[1];

      for (int i = 2; i < HAND_SIZE; i++) {
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
    if (!(*args->clients)[i]) {
      // fprintf(stderr, "skipping %d\n", i);
      continue;
    }

    struct pokeval_hand_t hand_tmp = {0};
    if (args->game_state->winner_declared && args->game_state->player_count != 1) {
      memcpy(&args->game_state->player[i].hand, &args->real_hand->player[i],
             sizeof(struct pokeval_hand_t));

    } else {
      memcpy(&hand_tmp, &args->game_state->player[i].hand, sizeof(struct pokeval_hand_t));
      memcpy(&args->game_state->player[i].hand, &args->real_hand->player[i],
             sizeof(struct pokeval_hand_t));
    }

    size_t size = 0;
    uint8_t *data = serialize_game_state(args->game_state, &size);
    if (!data)
      return;

    if (!args->game_state->winner_declared || args->game_state->player_count == 1)
      memcpy(&args->game_state->player[i].hand, &hand_tmp, sizeof(struct pokeval_hand_t));

    uint32_t size_net = htonl(size);

    // fprintf(stderr, "sending to %d\n", i);
    if (send_with_retries((*args->clients)[i], &size_net, sizeof(size_net)) == -1 ||
        send_with_retries((*args->clients)[i], data, size) == -1) {
      fprintf(stderr, "Failed to send game state to client %d after retries\n", i);
      if (handle_disconnections(*args->clients, *args->socket_set, *args->slot_taken,
                                args->game_state)) {
        broadcast_game_state(args);
      }
    }
    free(data);
  }
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
  int8_t id = args->game_state->turn_id;
  Player_t *recipient = &args->game_state->player[id];
  if (recipient->id == -1)
    recipient = get_next_connected_client(args->game_state->player, id);
  Player_t *start = recipient;

  do {
    id = recipient->id;
    puts(msg);
    TCPsocket sock = (*args->clients)[id];
    if (!sock)
      continue;

    if (send_status_message(sock, msg) < 0) {
      fprintf(stderr, "[broadcast_status_message] Failed to send to client %d\n", id);
    }
  } while ((recipient = get_next_connected_client(args->game_state->player, id)) != start);
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

int send_draw_prompt(TCPsocket sock) {
  uint8_t buffer[6]; // 4 bytes size + 2 bytes opcode

  uint32_t size = htonl(2); // payload is 2 bytes
  memcpy(buffer, &size, 4);

  buffer[4] = (MSG_DRAW_PROMPT >> 8) & 0xFF;
  buffer[5] = MSG_DRAW_PROMPT & 0xFF;

  int sent = send_all_tcp(sock, buffer, sizeof(buffer));
  printf("Sent draw prompt: %d bytes\n", sent);
  return sent;
}

static int recv_discards_send_new_cards(TCPsocket sock, DrawRequestMsg_t *out_req) {
  uint8_t buffer[7];

  int n_bytes = recv_all_tcp(sock, buffer, sizeof(buffer));
  if (n_bytes <= 0)
    return n_bytes;

  uint16_t opcode = (buffer[0] << 8) | buffer[1];
  if (opcode != MSG_DRAW_REQUEST)
    return -1;

  uint8_t count = buffer[2];
  if (count > MAX_DISCARDS)
    return -1;

  out_req->discard_count = count;
  memcpy(out_req->discard_indices, &buffer[3], MAX_DISCARDS); // copy all 4

  return sizeof(buffer); // 7 bytes
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

static void handle_draw(ArgsBroadcastGameState_t *args, TCPsocket sock, const int id,
                        DH_Deck *deck) {
  puts("sending draw prompt");
  send_draw_prompt(sock);

  DrawRequestMsg_t req;
  if (recv_discards_send_new_cards(sock, &req) <= 0) {
    fprintf(stderr, "Failed to receive draw request.\n");
    // handle disconnect or protocol error
  } else {
    printf("Player wants to discard %u cards: ", req.discard_count);
    for (int i = 0; i < req.discard_count; ++i) {
      printf("%u ", req.discard_indices[i]);
      args->real_hand->player[id].card[req.discard_indices[i]] = DH_deal_top_card(deck);
    }
    puts("");
  }
  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof status_str, "%s drew %d", args->game_state->player[id].nick,
           req.discard_count);
  send_new_hand(sock, &args->real_hand->player[id], HAND_SIZE);
  broadcast_status_message(args, status_str);
}

static EPlayerAction_t handle_check(Player_t *turn, PlayerActionMsg_t *action) {
  turn->has_checked = true;
  puts("player checks");
  action->str = "checks";
  return ACTION_CHECK;
}

static EPlayerAction_t handle_fold(ArgsBroadcastGameState_t *args, Player_t *turn,
                                   PlayerActionMsg_t *action) {
  turn->in = false;
  args->game_state->player_count--;
  action->str = "folds";
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
  struct pokeval_need_comparing_t need_comparing[pl_count];
  Player_t *ptr = starting_player;
  for (uint8_t i = 0; i < pl_count; i++) {
    need_comparing[i].won = false;
    need_comparing[i].id = ptr->id;
    memcpy(&need_comparing[i].hand, &args->real_hand->player[ptr->id],
           sizeof(struct pokeval_hand_t));
    ptr = get_next_player(players_array, ptr->id);
  }

  results->n_winners = pokeval_compare_hands(need_comparing, pl_count);
  uint8_t winners = 0;

  // Ties are not fully implemented yet. pokeval_compare_hands() handles them, but
  // the tests need to be reviewed and perhaps added to in the pokeval library. The code
  // here to report ties and distribute the pot to tied players isn't complete.
  for (int i = 0; i < pl_count; i++) {
    if (!need_comparing[i].won)
      continue;
    results->id[winners++] = need_comparing[i].id;
    Player_t *winner = &args->game_state->player[need_comparing[i].id];
    winner->winner = true;
    fprintf(stderr, "winner id: %d\n", need_comparing[i].id);
    char status_str[LEN_STATUS_STR];
    snprintf(status_str, sizeof(status_str),
             // When broadcast is called, it will reveal the cards if winner has been declared. We
             // don't need to call that yet, so using the values from "real_hand" for now
             "%s wins with %s\n", winner->nick,
             pokeval_ranks[pokeval_evaluate_hand(args->real_hand->player[winner->id])]);
    broadcast_status_message(args, status_str);
    uint32_t share = args->game_state->pot / results->n_winners;
    args->game_state->pot = args->game_state->pot % results->n_winners;
    winner->coins += share;
  }
  broadcast_game_state(args);
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

    Uint32 wait_ms = args->game_state->action_time_out_ms;
    Uint32 start = SDL_GetTicks();
    PlayerActionMsg_t action = {0};

    // If the player disconnects, their nick will be erased from the players array, however, it
    // will still be needed for the status message that reports their disconnect
    char nick[sizeof(turn->nick)] = {0};
    strcpy(nick, turn->nick);
    int8_t save_id = turn->id;

    while (SDL_GetTicks() - start < wait_ms) {
      // fprintf(stderr, "Waiting for action from %d\n", args->game_state->turn_id);
      SDLNet_CheckSockets(*args->socket_set, 100); // wait up to 100ms
      if (SDLNet_SocketReady((*args->clients)[turn->id])) {
        // puts("socket ready");
        // char tmp[sizeof args->game_state->status_str];
        if (recv_player_action((*args->clients)[turn->id], &action) > 0) {
          if (args->game_state->total_bets_plus_raises == 0) {
            switch (action.action) {
            case ACTION_CHECK:
              handle_check(turn, &action);
              break;
            case ACTION_BET:
              server_handle_bet(args->game_state, turn->id, action.amount);
              action.str = "bets ";
              break;
            case ACTION_FOLD:
              handle_fold(args, turn, &action);
              break;
            default:
              fprintf(stderr, "Invalid Action received\n");
              exit(EXIT_FAILURE);
            }
          } else {
            switch (action.action) {
            case ACTION_CALL:
              server_handle_call(args->game_state, turn->id);
              action.str = "calls";
              break;
            case ACTION_RAISE:
              server_handle_raise(args->game_state, turn->id, action.amount);
              action.str = "raises ";
              break;
            case ACTION_FOLD:
              handle_fold(args, turn, &action);
              break;
            default:
              fprintf(
                  stderr,
                  "Invalid Action received\nThe client is writing checks their body can't cash.\n");
              exit(EXIT_FAILURE);
            }
          }
        } else {
          remove_disconnected_player(*args->clients, *args->socket_set, *args->slot_taken, turn);
          args->game_state->player_count--;
          SDL_Delay(10);
        }
        break;
      }
      SDL_Delay(50); // avoid busy-waiting
    }

    char status_str[LEN_STATUS_STR] = {0};
    // The id will be -1 if the player disconnected when it was their turn to
    // send an action
    if (turn->id != -1) {
      if (action.action == 0) {
        if (!has_paid_all_bets(args->game_state, turn)) {
          action.action = handle_fold(args, turn, &action);
        } else if (args->game_state->total_bets_plus_raises == 0) {
          action.action = handle_check(turn, &action);
        }
      }

      if (action.amount > 0)
        snprintf(status_str, sizeof status_str, "%s %s%d\n", nick, action.str, action.amount);
      else
        snprintf(status_str, sizeof status_str, "%s %s\n", nick, action.str);
    } else
      snprintf(status_str, sizeof status_str, "%s disconnected\n", nick);

    broadcast_status_message(args, status_str);

    // use previously saved id here, in case the client disconnected
    turn = get_next_player(players_array, save_id);

    // fprintf(stderr, "player %d / total paid: %d\n", turn->id,
    // args->game_state->player[turn->id].total_paid);
    // fprintf(stderr, "total_bets_plus_raises: %d\n", args->game_state->total_bets_plus_raises);

    if (args->game_state->player_count == 1) { // All other players folded
      // broadcast_game_state(args);
      turn = starting_player;
      do {
        if (turn->in) {
          turn->winner = true;
          snprintf(status_str, sizeof(status_str), "%s wins\n", nick);
          broadcast_status_message(args, status_str);

          args->game_state->winner_declared = true;
          results.n_winners = 1;
          fprintf(stderr, "winner id from fold: %d\n", turn->id);
          results.id[0] = turn->id;
          turn->coins += args->game_state->pot;
          args->game_state->pot = 0;
          break;
        }
      } while ((turn = get_next_player(players_array, turn->id)) != starting_player);

    } else if (args->game_state->total_bets_plus_raises == 0) {
      if (turn == starting_player)
        break;
    } else if (has_paid_all_bets(args->game_state, turn)) {
      break; // Everyone either checked or paid all bets and raises
    }

    if (results.n_winners > 0) {
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

static void remove_disconnected_player(TCPsocket *clients, SDLNet_SocketSet socket_set,
                                       bool *slot_taken, Player_t *p) {
  const int id = p->id;
  if (SDLNet_TCP_DelSocket(socket_set, clients[id]) == -1) {
    fputs(SDLNet_GetError(), stderr);
    return;
  }

  printf("Client %d disconnected\n", id);
  SDLNet_TCP_Close(clients[id]);
  clients[id] = NULL;
  slot_taken[id] = false;

  // Reset player info
  memset(p->nick, 0, sizeof(p->nick));
  p->coins = STARTING_N_COINS;
  p->total_paid = 0;
  p->winner = false;
  p->has_checked = false;
  p->in = false;
  p->id = -1;
}

static bool handle_disconnections(TCPsocket *clients, SDLNet_SocketSet socket_set, bool *slot_taken,
                                  GameState_t *game_state) {
  bool someone_disconnected = false;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!slot_taken[i])
      continue;

    if (SDLNet_SocketReady(clients[i])) {
      char tmp;
      int result = SDLNet_TCP_Recv(clients[i], &tmp, 1);
      if (result <= 0) {
        remove_disconnected_player(clients, socket_set, slot_taken, &game_state->player[i]);
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

  uint8_t dealer_id = args->game_state->dealer_id;
  Player_t *starting_player = get_next_player(players_array, dealer_id);
  server_handle_ante(args->game_state, 250);

  Player_t *turn = starting_player;
  RoundResults results = {0};
  for (int i = 0; i < n_betting_rounds; i++) {
    results = handle_round();
    if (results.n_winners > 0 || i == draws)
      break;

    turn = starting_player;
    do {
      args->game_state->turn_id = turn->id;

      // The player's new cards are sent to them in handle_draw(), but
      // the clients use game_state.turn_id to decide which buttons to display.
      // It would be better if that behavior were changed so that broadcasting the
      // entire game state wasn't required here (the only info that needs updating
      // at this point is turn_id).
      broadcast_game_state(args);

      handle_draw(args, (*args->clients)[turn->id], turn->id, deck);
    } while ((turn = get_next_player(players_array, turn->id)) != starting_player);
    broadcast_game_state(args);
  }
  determine_winner(args, &results);
}

void game_five_card_stud(GAME_ARGS) {

  Player_t *starting_player = get_next_player(players_array, args->game_state->dealer_id);
  Player_t *turn = starting_player;
  RoundResults results = {0};
  for (int i = 0; i < n_betting_rounds; i++) {
    results = handle_round();

    if (results.n_winners > 0 || i == draws)
      break;

    printf("round: %d\n", i);
    do {
      int id = turn->id;
      struct pokeval_hand_t *hand = &turn->hand;
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

  Player_t *players_array = args->game_state->player;
  *args->real_hand = deal_cards_to_players(args->game_state, deck, game_type);
  args->game_state->winner_declared = false;
  args->game_state->player_count = count_active_clients(*args->slot_taken);
  fprintf(stderr, "player count: %d\n", args->game_state->player_count);
  args->game_state->total_bets_plus_raises = 0;
  args->game_state->winner_declared = false;

  // Using function pointers...
  const GameChoice_t *choice = find_game_choice_by_type(game_type);
  if (choice && choice->func) {
    choice->func(args, players_array, deck, choice->n_betting_rounds, choice->draws);
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
      strncpy(player->nick, candidate, sizeof(player->nick));
      player->nick[sizeof(player->nick) - 1] = '\0';
      return;
    }

    suffix++;
  }

  fprintf(stderr, "Could not find a unique nickname after %d attempts\n", suffix_limit);
}

static EReturnCode_t receive_game_type_and_run_game(ArgsBroadcastGameState_t *args, DH_Deck *deck) {
  uint8_t game_type;
  int8_t *dealer_id = &args->game_state->dealer_id;
  if (recv_game_select((*args->clients)[*dealer_id], &game_type) == SIZE_MESSAGE_GAME_SELECT) {
    fprintf(stderr, "Client chose game type: 0x%02x\n", game_type);
  } else {
    fprintf(stderr, "Dealer failed to send valid game type or disconnected.\n");
    remove_disconnected_player(*args->clients, *args->socket_set, *args->slot_taken,
                               &args->game_state->player[*dealer_id]);
    broadcast_game_state(args);
    SDL_Delay(10);
    return RC_ERR;
  }

  printf("All %d players are ready. Starting game.\n", count_active_clients(*args->slot_taken));
  args->game_state->at_menu = false;

  play_game(game_type, args, deck);

  broadcast_game_state(args);

  // Uint32 wait_ms = args->game_state->end_of_round_time_out_ms;
  Uint32 wait_ms = 5000;
  Uint32 start = SDL_GetTicks();
  while (SDL_GetTicks() - start < wait_ms)
    ;
  args->game_state->at_menu = true;

  reset_players(args->game_state);

  // Rotate dealer to next active client
  int next_dealer = get_next_dealer(*dealer_id, *args->slot_taken);
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

int run_server(const char *bind_address, Path_t *path, const bool test_mode) {
  GameState_t game_state = {0};
  Config_t config = init_game_state(&game_state, path, test_mode);
  game_state.pot = 0;

  if (SDL_Init(0) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL or SDL_net init failed: %s\n", SDLNet_GetError());
    return 1;
  }

  IPaddress ip;
  char *host = config.bind_address;
  if (!bind_address) {
    // ip.host = SDL_SwapBE32(INADDR_LOOPBACK);  // 127.0.0.1
    // ip.port = SDL_SwapBE16(default_port);
    host = config.bind_address;
    if (strcmp(config.bind_address, "NULL") == 0)
      host = NULL;
  } else
    host = (char *)bind_address;
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

  if (!test_mode)
    DH_pcg_srand_auto();
  else
    DH_pcg_srand(1, 1);

  int game_started = 0;
  RealHand_t real_hand = {0};

  bool slot_taken[MAX_CLIENTS] = {false};
  while (!game_started) {
    ArgsBroadcastGameState_t args_broadcast_game_state = {
        .clients = &clients,
        .socket_set = &socket_set,
        .game_state = &game_state,
        .real_hand = &real_hand,
        .slot_taken = &slot_taken,
    };

    uint8_t active_clients = count_active_clients(slot_taken);
    int8_t *dealer_id = &game_state.dealer_id;
    int num_ready = SDLNet_CheckSockets(socket_set, 0);
    if (num_ready > 0) {
      if (active_clients > 1 && SDLNet_SocketReady(clients[*dealer_id]) &&
          game_state.player[*dealer_id].id != -1) {
        if (receive_game_type_and_run_game(&args_broadcast_game_state, &deck) == RC_ERR)
          continue;
      } else if (handle_disconnections(clients, socket_set, slot_taken, &game_state))
        broadcast_game_state(&args_broadcast_game_state);
    }
    // broadcast_game_state(&args_broadcast_game_state);

    TCPsocket new_client = SDLNet_TCP_Accept(server);
    if (new_client) {
      int slot = -1;
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!slot_taken[i]) {
          slot = i;
          break;
        }
      }

      if (slot != -1) {
        clients[slot] = new_client;
        slot_taken[slot] = true;
        SDLNet_TCP_AddSocket(socket_set, new_client);

        IPaddress *remote_ip = SDLNet_TCP_GetPeerAddress(new_client);
        if (remote_ip) {
          Uint32 ipaddr = SDL_SwapBE32(remote_ip->host);
          Uint16 port = SDL_SwapBE16(remote_ip->port);
          printf("Client %d connected from %d.%d.%d.%d:%d\n", slot, (ipaddr >> 24) & 0xFF,
                 (ipaddr >> 16) & 0xFF, (ipaddr >> 8) & 0xFF, ipaddr & 0xFF, port);
        }

        Player_t *slot_id = &game_state.player[slot];
        slot_id->id = slot;
        slot_id->in = true;

        int32_t net_player_id = htonl(slot);
        send_all_tcp(new_client, &net_player_id, sizeof(int32_t));

        if (!test_mode) {
          Player_t *player = &game_state.player[slot];
          int32_t net_len;

          // Step 1: Recv the size first (must happen before interpreting it)
          if (recv_all_tcp(new_client, &net_len, sizeof(int32_t)) <= 0) {
            // Handle error: client disconnected or invalid
            fprintf(stderr, "Failed to receive nickname length.\n");
            SDLNet_TCP_Close(new_client);
            player->id = -1;
            player->in = false;
            slot_taken[slot] = false;
            break;
          }

          // Step 2: Now convert
          size_t len = ntohl(net_len);

          // Step 3: Validate length
          if (len == 0 || len >= sizeof(player->nick)) {
            fprintf(stderr, "Invalid nickname length: %zu\n", len);
            SDLNet_TCP_Close(new_client);
            player->id = -1;
            player->in = false;
            slot_taken[slot] = false;
            break;
          }

          // Step 4: Read nickname
          memset(player->nick, 0, sizeof(player->nick));
          if (recv_all_tcp(new_client, player->nick, len) != (ssize_t)len) {
            fprintf(stderr, "Failed to receive nickname.\n");
            SDLNet_TCP_Close(new_client);
            player->id = -1;
            player->in = false;
            slot_taken[slot] = false;
            break;
          }

          // Step 5: Null terminate
          player->nick[len] = '\0';
          printf("received nick: %s\n", player->nick);
          ensure_unique_nick(&game_state, player, slot);
        }

        broadcast_game_state(&args_broadcast_game_state);
      } else {
        printf("Server full. Rejecting connection.\n");
        SDLNet_TCP_Close(new_client);
      }
    }

    if (*dealer_id == -1) {
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (slot_taken[i]) {
          *dealer_id = i;
          printf("Initial dealer set to player %d\n", i);
          break;
        }
      }
    }

    if (active_clients > 1) {
      int result = SDLNet_CheckSockets(socket_set, 50);
      if (result == -1) {
        fputs(SDLNet_GetError(), stderr);
        continue;
      }

      if (reassign_dealer_if_needed(&game_state, slot_taken))
        broadcast_game_state(&args_broadcast_game_state);

      if (*dealer_id == -1)
        continue; // No valid dealer

      // if (SDLNet_SocketReady(clients[*dealer_id])) {
      // if (receive_game_type_and_run_game(&args_broadcast_game_state, &deck) == RC_ERR)
      // continue;
      //}
    }
    // if (SDLNet_CheckSockets(socket_set, 0) > 0)
    // if (!SDLNet_SocketReady(clients[*dealer_id]))
    // if (handle_disconnections(clients, socket_set, slot_taken, &game_state))
    // broadcast_game_state(&args_broadcast_game_state);
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
