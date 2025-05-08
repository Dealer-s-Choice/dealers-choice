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
  struct hand_t hand[MAX_PLAYERS];
  bool face_down[HAND_SIZE];
};

// On Windows, this is defined in <ws2tcpip.h>. Rather than include the file
// let's just do this...
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

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

static void init_game_state(struct game_state_t *game_state) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    game_state->player[i] = (struct player_t){.id = -1, .chips = 20000};
    snprintf(game_state->player[i].name, sizeof game_state->player[i].name, "Player %d", i);
  }

  game_state->dealer_id = 0;
  game_state->current_bet = 0;
  game_state->at_menu = true;
}

static struct fow_t deal_cards_to_players(struct game_state_t *game_state,
                                          const struct dh_deck *deck) {
  struct fow_t fow = {0};
  for (int p = 0; p < MAX_PLAYERS; ++p) {
    if (game_state->player[p].id != -1) {
      for (int i = 0; i < HAND_SIZE; ++i) {
        game_state->player[p].hand.card[i] = dh_card_back;
        fow.hand[p].card[i] = deck->card[i + HAND_SIZE * p];
      }
    }
  }
  return fow;
}

static void broadcast_game_state(TCPsocket *clients, int client_count,
                                 struct game_state_t *game_state, struct fow_t *fow) {
  for (int i = 0; i < client_count; ++i) {
    if (!clients[i])
      continue;

    struct hand_t hand_tmp;
    memcpy(&hand_tmp, &game_state->player[i].hand, sizeof(struct hand_t));
    memcpy(&game_state->player[i].hand, &fow->hand[i], sizeof(struct hand_t));

    size_t size = 0;
    uint8_t *data = serialize_game_state(game_state, &size);
    if (!data)
      return;

    memcpy(&game_state->player[i].hand, &hand_tmp, sizeof(struct hand_t));

    uint32_t size_net = htonl(size);

    if (send_all_tcp(clients[i], &size_net, sizeof(size_net)) == -1 ||
        send_all_tcp(clients[i], data, size) == -1) {
      fprintf(stderr, "Failed to send game state to client %d\n", i);
      SDLNet_TCP_Close(clients[i]);
      clients[i] = NULL;
    }
    free(data);
  }
}

// Returns 0 on success, -1 on error, fills *out_game_type
static int8_t recv_game_select(TCPsocket sock, uint8_t *out_game_type) {
  uint8_t buffer[3];

  if (recv_all_tcp(sock, buffer, sizeof(buffer)) != 0)
    return -1;

  uint16_t opcode = (buffer[0] << 8) | buffer[1];
  if (opcode != MSG_GAME_SELECT)
    return -1;

  *out_game_type = buffer[2];
  return 0;
}

static int8_t recv_player_action(TCPsocket sock, struct player_action_msg_t *out_action) {
  uint8_t buffer[7];

  if (recv_all_tcp(sock, buffer, sizeof(buffer)) != 0)
    return -1;

  uint16_t opcode = (buffer[0] << 8) | buffer[1];
  if (opcode != MSG_PLAYER_ACTION)
    return -1;

  out_action->action = buffer[2];
  out_action->amount = ((uint32_t)buffer[3] << 24) | ((uint32_t)buffer[4] << 16) |
                       ((uint32_t)buffer[5] << 8) | ((uint32_t)buffer[6]);

  return 0;
}

int run_server(void) {
  struct game_state_t game_state = {0};
  init_game_state(&game_state);
  game_state.pot = 0;

  if (SDL_Init(0) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL or SDL_net init failed: %s\n", SDLNet_GetError());
    return 1;
  }

  IPaddress ip;
  if (SDLNet_ResolveHost(&ip, NULL, 61357) == -1) {
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

  struct dh_deck deck;
  dh_init_deck(&deck);
  dh_pcg_srand_auto();
  dh_shuffle_deck(&deck);

  int game_started = 0;
  int client_count = 0;

  struct fow_t fow = {0};

  while (!game_started) {
    TCPsocket new_client = SDLNet_TCP_Accept(server);
    if (new_client && client_count < MAX_CLIENTS) {
      clients[client_count] = new_client;
      SDLNet_TCP_AddSocket(socket_set, new_client);

      IPaddress *remote_ip = SDLNet_TCP_GetPeerAddress(new_client);
      if (remote_ip) {
        Uint32 ipaddr = SDL_SwapBE32(remote_ip->host);
        Uint16 port = SDL_SwapBE16(remote_ip->port);
        printf("Client %d connected from %d.%d.%d.%d:%d\n", client_count, (ipaddr >> 24) & 0xFF,
               (ipaddr >> 16) & 0xFF, (ipaddr >> 8) & 0xFF, ipaddr & 0xFF, port);
      }

      game_state.player[client_count].id = client_count;

      int32_t net_player_id = htonl(client_count);
      send_all_tcp(new_client, &net_player_id, sizeof(int32_t));

      broadcast_game_state(clients, client_count + 1, &game_state, &fow);

      client_count++;
    }

    if (client_count >= 2) {
      SDLNet_CheckSockets(socket_set, 0);
      if (SDLNet_SocketReady(clients[game_state.dealer_id])) {
        uint8_t game_type;
        if (recv_game_select(clients[game_state.dealer_id], &game_type) == 0)
          printf("Client chose game type: 0x%02x\n", game_type);
        else {
          fprintf(stderr, "Invalid game type sent: 0x%02x\n", game_type);
          continue;
        }
        printf("All %d players are ready. Starting game.\n", client_count);
        game_state.at_menu = false;
        fow = deal_cards_to_players(&game_state, &deck);

        do {
          game_state.turn_id =
              game_state.dealer_id < MAX_PLAYERS - 1 ? game_state.dealer_id + 1 : 0;
          printf("turn_id = %d\n", game_state.turn_id);
        } while (game_state.player[game_state.turn_id].id == -1); // TODO: Fix condition!
        broadcast_game_state(clients, client_count, &game_state, &fow);

        Uint32 wait_ms = 5000; // wait up to 5 seconds
        Uint32 start = SDL_GetTicks();
        while (SDL_GetTicks() - start < wait_ms) {
          SDLNet_CheckSockets(socket_set, 100); // wait up to 100ms
          if (SDLNet_SocketReady(clients[game_state.turn_id])) {
            puts("socket ready");

            struct player_action_msg_t action;
            if (recv_player_action(clients[game_state.turn_id], &action) == 0) {
              printf("Received action %u with amount %u\n", action.action, action.amount);
              game_state.player[game_state.turn_id].chips -= action.amount;
              game_state.pot += action.amount;
              broadcast_game_state(clients, client_count, &game_state, &fow);
            } else {
              fprintf(stderr, "Failed to receive player action\n");
            }

            break;
          }
          SDL_Delay(50); // avoid busy-waiting
        }
      }
    }

    SDL_Delay(50);
  }

  for (int i = 0; i < client_count; i++) {
    SDLNet_TCP_Close(clients[i]);
  }
  SDLNet_TCP_Close(server);
  SDLNet_FreeSocketSet(socket_set);
  SDLNet_Quit();
  SDL_Quit();

  return 0;
}
