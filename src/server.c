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

#include <SDL2/SDL.h>
#include <deckhandler.h>
#include <pokeval.h>
#include <stdio.h>
#include <string.h>

#include "server.h"

#define MAX_CLIENTS 5

#include <SDL2/SDL_net.h>
#include <stdio.h>

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
  const struct preset_player_pos_t preset_player_pos = {
      .pos = {
          // P0: bottom center
          {.x = WINDOW_WIDTH / 3, .y = WINDOW_HEIGHT - 80},

          // P1: left, 1/3 down
          {.x = 20, .y = WINDOW_HEIGHT / 3},

          // P2: top-left
          {.x = 20, .y = 20},

          // P3: top-right
          {.x = WINDOW_WIDTH / 2 + 20, .y = 35},

          // P4: right, 1/3 down
          {.x = WINDOW_WIDTH / 2 + 20, .y = WINDOW_HEIGHT / 3 + 15},
      }};

  // This offers only a little extra protection if changes are made.
  _Static_assert(sizeof(preset_player_pos.pos) / sizeof(preset_player_pos.pos[0]) == 5,
                 "preset_player_pos.pos has wrong number of elements");

  for (int i = 0; i < MAX_PLAYERS; i++) {
    game_state->player[i] =
        (struct player_t){.id = -1, .pos = preset_player_pos.pos[i], .chips = 20000};
    snprintf(game_state->player[i].name, sizeof game_state->player[i].name, "Player %d", i);
  }

  game_state->dealer_id = 0;
  game_state->at_menu = true;
}

static void deal_cards_to_players(struct game_state_t *game_state, const struct dh_deck *deck) {
  for (int p = 0; p < MAX_PLAYERS; ++p) {
    if (game_state->player[p].id != -1) {
      for (int i = 0; i < HAND_SIZE; ++i) {
        game_state->player[p].hand.card[i] = deck->card[i + HAND_SIZE * p];
      }
    }
  }
}

static void broadcast_game_state(TCPsocket *clients, int client_count,
                                 const struct game_state_t *game_state) {
  size_t size = 0;
  uint8_t *data = serialize_game_state(game_state, &size);
  if (!data)
    return;

  uint32_t size_net = htonl(size);

  for (int i = 0; i < client_count; ++i) {
    if (!clients[i])
      continue;

    if (send_all_tcp(clients[i], &size_net, sizeof(size_net)) == -1 ||
        send_all_tcp(clients[i], data, size) == -1) {
      fprintf(stderr, "Failed to send game state to client %d\n", i);
      SDLNet_TCP_Close(clients[i]);
      clients[i] = NULL;
    }
  }

  free(data);
}

int run_server(void) {
  struct game_state_t game_state = {0};
  init_game_state(&game_state);
  game_state.pot = 500;

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

      broadcast_game_state(clients, client_count + 1, &game_state);

      client_count++;
    }

    if (client_count >= 2) {
      SDLNet_CheckSockets(socket_set, 0);
      if (SDLNet_SocketReady(clients[game_state.dealer_id])) {
        uint8_t msg;
        if (recv_all_tcp(clients[game_state.dealer_id], &msg, sizeof(msg)) != 0)
          continue;

        if (msg == 0x01) { // "start" signal from player
          printf("All %d players are ready. Starting game.\n", client_count);
          game_state.at_menu = false;
          deal_cards_to_players(&game_state, &deck);
          broadcast_game_state(clients, client_count, &game_state);
        } else
          puts("msg incorrect");
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
