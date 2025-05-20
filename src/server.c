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

#include <deckhandler.h>
#include <pokeval.h>
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "server.h"

#define MAX_CLIENTS 5

struct fow_t {
  struct pokeval_hand_t hand[MAX_PLAYERS];
  bool face_down[HAND_SIZE];
};

typedef void (*count_active_clients_p)(const bool *slot_taken);

typedef struct {
  TCPsocket (*clients)[MAX_CLIENTS];
  SDLNet_SocketSet *socket_set;
  int *active_clients;
  game_state_t *game_state;
  struct fow_t *fow;
  bool (*slot_taken)[MAX_CLIENTS];
} args_broadcast_game_state_t;

// typedef struct {
// SDLNet_SocketSet *socket_set;
// struct player_list_t *dealer;
// args_broadcast_game_state_t *broadcast_args;
//} args_handle_round_t;

// On Windows, this is defined in <ws2tcpip.h>. Rather than include the file
// let's just do this...
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

static void remove_disconnected_player(TCPsocket *clients, SDLNet_SocketSet socket_set,
                                       bool *slot_taken, game_state_t *game_state, const int i);

static bool handle_disconnections(TCPsocket *clients, SDLNet_SocketSet socket_set, bool *slot_taken,
                                  game_state_t *game_state);
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

void init_game_state(game_state_t *game_state) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    game_state->player[i] = (struct player_t){
        .id = -1,
        .coins = 20000,
        .in = false,
        .total_paid = 0,
        .winner = false,
        .has_checked = false,
    };
    snprintf(game_state->player[i].name, sizeof game_state->player[i].name, "Player %d", i);
  }

  game_state->dealer_id = 0;
  game_state->at_menu = true;
  game_state->player_count = 0;
  game_state->total_bets_plus_raises = 0;
  game_state->winner_declared = false;
  game_state->round_over = true;
}

static struct fow_t deal_cards_to_players(game_state_t *game_state, struct dh_deck *deck,
                                          struct player_list_t *active_players) {
  struct fow_t fow = {0};
  struct player_list_t *dealer = active_players;

  do {
    int id = active_players->id;
    for (int i = 0; i < HAND_SIZE; ++i) {
      game_state->player[id].hand.card[i] = dh_card_back;
      fow.hand[id].card[i] = dh_deal_top_card(deck);
    }
    active_players = active_players->next;
  } while (active_players != dealer);

  return fow;
}

static void broadcast_game_state(args_broadcast_game_state_t *args) {
  for (int i = 0; i < *args->active_clients; ++i) {
    if (!(*args->clients)[i])
      continue;

    struct pokeval_hand_t hand_tmp = {0};
    if (args->game_state->winner_declared && args->game_state->player_count != 1) {
      for (int z = 0; z < *args->active_clients; z++) {
        memcpy(&args->game_state->player[z].hand, &args->fow->hand[z],
               sizeof(struct pokeval_hand_t));
      }
    } else {
      memcpy(&hand_tmp, &args->game_state->player[i].hand, sizeof(struct pokeval_hand_t));
      memcpy(&args->game_state->player[i].hand, &args->fow->hand[i], sizeof(struct pokeval_hand_t));
    }

    size_t size = 0;
    uint8_t *data = serialize_game_state(args->game_state, &size);
    if (!data)
      return;

    if (!args->game_state->winner_declared || args->game_state->player_count == 1)
      memcpy(&args->game_state->player[i].hand, &hand_tmp, sizeof(struct pokeval_hand_t));

    uint32_t size_net = htonl(size);

    if (send_all_tcp((*args->clients)[i], &size_net, sizeof(size_net)) == -1 ||
        send_all_tcp((*args->clients)[i], data, size) == -1) {
      fprintf(stderr, "Failed to send game state to client %d\n", i);
      // TODO: Implement retries
      if (handle_disconnections(*args->clients, *args->socket_set, *args->slot_taken,
                                args->game_state))
        broadcast_game_state(args);
    }
    free(data);
  }
}

// Returns 0 on success, -1 on error, fills *out_game_type
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

static int recv_player_action(TCPsocket sock, struct player_action_msg_t *out_action) {
  uint8_t buffer[7];

  int n_bytes;
  if ((n_bytes = recv_all_tcp(sock, buffer, sizeof(buffer))) <= 0)
    return n_bytes;

  uint16_t opcode = (buffer[0] << 8) | buffer[1];
  if (opcode != MSG_PLAYER_ACTION)
    return -1;

  out_action->action = buffer[2];
  out_action->amount = ((uint32_t)buffer[3] << 24) | ((uint32_t)buffer[4] << 16) |
                       ((uint32_t)buffer[5] << 8) | ((uint32_t)buffer[6]);

  return n_bytes;
}

static void server_handle_call(game_state_t *game_state, const uint8_t turn_id) {
  uint32_t owed = game_state->total_bets_plus_raises - game_state->player[turn_id].total_paid;
  game_state->player[turn_id].coins -= owed;
  game_state->player[turn_id].total_paid += owed;
  game_state->pot += owed;
}

static void server_handle_ante(game_state_t *game_state, struct player_list_t *head,
                               const uint32_t amount) {
  struct player_list_t *turn = head;
  do {
    if (game_state->player[turn->id].in) {
      game_state->player[turn->id].coins -= amount;
      game_state->pot += amount;
    }
  } while ((turn = turn->next) != head);
}

static void server_handle_bet(game_state_t *game_state, const uint8_t turn_id,
                              const uint32_t amount) {
  game_state->player[turn_id].coins -= amount;
  game_state->player[turn_id].total_paid += amount;
  game_state->total_bets_plus_raises += amount;
  game_state->pot += amount;
}

// On Ubuntu 24.04 arm64: error: conflicting types for ‘raise’; so I've given
// this a more unique name now
static void server_handle_raise(game_state_t *game_state, const uint8_t turn_id,
                                const uint32_t amount) {
  server_handle_call(game_state, turn_id);
  server_handle_bet(game_state, turn_id, amount);
}

static void handle_round(SDLNet_SocketSet socket_set, args_broadcast_game_state_t *args,
                         struct player_list_t *dealer) {
  struct player_list_t *starting_player = dealer->next;
  struct player_list_t *turn = starting_player;

  do {
    args->game_state->round_over = false;
    args->game_state->turn_id = turn->id;
    fprintf(stderr, "Waiting for action from %d\n", args->game_state->turn_id);
    broadcast_game_state(args);

    Uint32 wait_ms = 20000; // wait up to 20 seconds
    Uint32 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < wait_ms) {
      SDLNet_CheckSockets(socket_set, 100); // wait up to 100ms
      if (SDLNet_SocketReady((*args->clients)[args->game_state->turn_id])) {
        puts("socket ready");

        struct player_action_msg_t action;
        if (recv_player_action((*args->clients)[args->game_state->turn_id], &action) > 0) {
          printf("Received action %u with amount %u\n", action.action, action.amount);
          uint8_t turn_id = args->game_state->turn_id;
          switch (action.action) {
          case ACTION_CHECK:
            args->game_state->player[turn_id].has_checked = true;
            break;
          case ACTION_BET:
            server_handle_bet(args->game_state, turn_id, action.amount);
            break;
          case ACTION_FOLD:
            args->game_state->player[turn_id].in = false;
            args->game_state->player_count--;
            break;
          case ACTION_CALL:
            server_handle_call(args->game_state, turn_id);
            break;
          case ACTION_RAISE:
            server_handle_raise(args->game_state, turn_id, action.amount);
            break;
          default:
            fprintf(stderr, "Invalid Action received\n");
          }
        } else {
          fprintf(stderr, "Failed to receive player action\n");
        }

        break;
      }
      SDL_Delay(50); // avoid busy-waiting
    }
    turn = turn->next;

    fprintf(stderr, "player %d / total paid: %d\n", turn->id,
            args->game_state->player[turn->id].total_paid);
    fprintf(stderr, "total_bets_plus_raises: %d\n", args->game_state->total_bets_plus_raises);

    if (args->game_state->player_count == 1) {
      struct player_list_t *ptr = turn;
      if (args->game_state->player[ptr->id].in) {
        args->game_state->player[ptr->id].winner = true;
        args->game_state->player[ptr->id].coins += args->game_state->pot;
        args->game_state->pot = 0;
        break;
      }
      ptr = ptr->next;
    }

    if (args->game_state->total_bets_plus_raises == 0) {
      // Everyone checked
      if (turn == starting_player)
        break;
    } else if (args->game_state->total_bets_plus_raises ==
               args->game_state->player[turn->id].total_paid)
      break;

  } while (true);
  if (args->game_state->player_count != 1) {
    uint8_t pl_count = args->game_state->player_count;
    struct pokeval_need_comparing_t need_comparing[pl_count];
    struct player_list_t *ptr = starting_player;
    for (uint8_t i = 0; i < pl_count; i++) {
      need_comparing[i].won = false;
      need_comparing[i].id = ptr->id;
      memcpy(&need_comparing[i].hand, &args->fow->hand[ptr->id], sizeof(struct pokeval_hand_t));
      ptr = ptr->next;
    }

    uint8_t num_winners = pokeval_compare_hands(need_comparing, pl_count);
    for (int i = 0; i < pl_count; i++) {
      if (!need_comparing[i].won)
        continue;
      args->game_state->player[need_comparing[i].id].winner = true;
      fprintf(stderr, "winner id: %d\n", need_comparing[i].id);
      uint32_t share = args->game_state->pot / num_winners;
      args->game_state->pot = args->game_state->pot % num_winners;
      args->game_state->player[need_comparing[i].id].coins += share;
    }
  }
  args->game_state->winner_declared = true;
  args->game_state->round_over = true;
}

static void reset_players(game_state_t *game_state) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (game_state->player[i].id == -1)
      continue;
    game_state->player[i].in = true;
    game_state->player[i].total_paid = 0;
    game_state->player[i].winner = false;
    game_state->player[i].has_checked = false;
  }
}

static void remove_disconnected_player(TCPsocket *clients, SDLNet_SocketSet socket_set,
                                       bool *slot_taken, game_state_t *game_state, const int i) {
  if (SDLNet_TCP_DelSocket(socket_set, clients[i]) == -1) {
    puts(SDL_GetError());
    return;
  }

  printf("Client %d disconnected\n", i);
  SDLNet_TCP_Close(clients[i]);
  clients[i] = NULL;
  slot_taken[i] = false;

  // Reset player info
  game_state->player[i].total_paid = 0;
  game_state->player[i].winner = false;
  game_state->player[i].has_checked = false;
  game_state->player[i].in = false;
  game_state->player[i].id = -1;
}

static bool handle_disconnections(TCPsocket *clients, SDLNet_SocketSet socket_set, bool *slot_taken,
                                  game_state_t *game_state) {
  bool someone_disconnected = false;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!slot_taken[i])
      continue;

    if (SDLNet_SocketReady(clients[i])) {
      char tmp;
      int result = SDLNet_TCP_Recv(clients[i], &tmp, 1);
      if (result <= 0) {
        remove_disconnected_player(clients, socket_set, slot_taken, game_state, i);
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

static int count_active_clients(const bool *slot_taken) {
  int count = 0;
  for (int i = 0; i < MAX_CLIENTS; i++)
    if (slot_taken[i])
      count++;
  return count;
}

static bool reassign_dealer_if_needed(game_state_t *game_state, bool *slot_taken) {
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

static void game_five_card_draw(args_broadcast_game_state_t *args, SDLNet_SocketSet socket_set,
                                struct player_list_t *dealer) {
  server_handle_ante(args->game_state, dealer, 250);
  handle_round(socket_set, args, dealer);
}

int run_server(void) {
  game_state_t game_state = {0};
  init_game_state(&game_state);
  game_state.pot = 0;

  if (SDL_Init(0) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL or SDL_net init failed: %s\n", SDLNet_GetError());
    return 1;
  }

  IPaddress ip;
  if (SDLNet_ResolveHost(&ip, NULL, default_port) == -1) {
    fprintf(stderr, "Failed to resolve host: %s\n", SDLNet_GetError());
    SDLNet_Quit();
    SDL_Quit();
    return 1;
  }

  TCPsocket server = SDLNet_TCP_Open(&ip);
  if (!server) {
    fprintf(stderr, "Failed to open server socket: %s\n", SDLNet_GetError());
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

  struct dh_deck deck = dh_get_new_deck();
  dh_pcg_srand_auto();

  int game_started = 0;
  struct fow_t fow = {0};

  int active_clients = 0;
  bool slot_taken[MAX_CLIENTS] = {false};
  while (!game_started) {
    args_broadcast_game_state_t args_broadcast_game_state = {
        .clients = &clients,
        .socket_set = &socket_set,
        .active_clients = &active_clients,
        .game_state = &game_state,
        .fow = &fow,
        .slot_taken = &slot_taken,
    };

    int num_ready = SDLNet_CheckSockets(socket_set, 0);
    if (num_ready > 0)
      if (handle_disconnections(clients, socket_set, slot_taken, &game_state) && active_clients > 0)
        broadcast_game_state(&args_broadcast_game_state);

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

        game_state.player[slot].id = slot;
        game_state.player[slot].in = true;

        int32_t net_player_id = htonl(slot);
        send_all_tcp(new_client, &net_player_id, sizeof(int32_t));

        // Count how many clients are currently connected
        active_clients = count_active_clients(slot_taken);

        broadcast_game_state(&args_broadcast_game_state);
      } else {
        printf("Server full. Rejecting connection.\n");
        SDLNet_TCP_Close(new_client);
      }
    }

    if (game_state.dealer_id == -1) {
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (slot_taken[i]) {
          game_state.dealer_id = i;
          printf("Initial dealer set to player %d\n", i);
          break;
        }
      }
    }

    while (true) {
      int result = SDLNet_CheckSockets(socket_set, 50);
      if (result == -1) {
        fputs(SDLNet_GetError(), stderr);
        break;
      }

      if ((active_clients = count_active_clients(slot_taken)) < 2)
        break;

      // printf("Active players >= 2\n");

      if (reassign_dealer_if_needed(&game_state, slot_taken))
        broadcast_game_state(&args_broadcast_game_state);

      if (game_state.dealer_id == -1)
        break; // No valid dealer

      // printf("Checking if dealer %d socket is ready...\n", game_state.dealer_id);
      if (SDLNet_SocketReady(clients[game_state.dealer_id])) {
        uint8_t game_type;
        if (recv_game_select(clients[game_state.dealer_id], &game_type) ==
            SIZE_MESSAGE_GAME_SELECT) {
          printf("Client chose game type: 0x%02x\n", game_type);
        } else {
          fprintf(stderr, "Dealer failed to send valid game type or disconnected.\n");
          remove_disconnected_player(clients, socket_set, slot_taken, &game_state,
                                     game_state.dealer_id);
          broadcast_game_state(&args_broadcast_game_state);
          SDL_Delay(10);
          continue;
        }

        printf("All %d players are ready. Starting game.\n", active_clients);
        game_state.at_menu = false;
        struct player_list_t *active_players = create_player_list(&game_state);
        if (!active_players)
          exit(EXIT_FAILURE);

        dh_shuffle_deck(&deck);
        struct player_list_t *dealer = active_players;
        do {
          if (dealer->id == game_state.dealer_id)
            break;
        } while ((dealer = dealer->next));

        fow = deal_cards_to_players(&game_state, &deck, active_players);
        game_state.winner_declared = false;
        game_state.total_bets_plus_raises = 0;

        switch (game_type) {
        case GAME_5_CARD_DRAW:
          game_five_card_draw(&args_broadcast_game_state, socket_set, dealer);
          break;
        default:
          break;
        }

        broadcast_game_state(&args_broadcast_game_state);
        free_player_list(active_players);

        Uint32 wait_ms = 10000; // wait up to 10 seconds before presenting the game menu
        Uint32 start = SDL_GetTicks();
        while (SDL_GetTicks() - start < wait_ms)
          game_state.at_menu = true;

        reset_players(&game_state);
        broadcast_game_state(&args_broadcast_game_state);

        // Rotate dealer to next active client
        int next_dealer = get_next_dealer(game_state.dealer_id, slot_taken);
        if (next_dealer != -1) {
          game_state.dealer_id = next_dealer;
          broadcast_game_state(&args_broadcast_game_state);
          printf("Dealer rotated to player %d\n", next_dealer);
        } else {
          printf("No valid dealer found after rotation\n");
          game_state.dealer_id = -1;
        }
      }
      if (handle_disconnections(clients, socket_set, slot_taken, &game_state))
        broadcast_game_state(&args_broadcast_game_state);
    }
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
