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
#include <SDL2/SDL_net.h>
#include <deckhandler.h>
#include <pokeval.h>
#include <stdio.h>
#include <string.h>

#include "server.h"

#define MAX_CLIENTS 5

static void init_players(struct player_t *player) {
  const struct preset_player_pos_t preset_player_pos = {
      .pos = {
          // P0: bottom center
          {.x = WINDOW_WIDTH / 3, .y = WINDOW_HEIGHT - 80},

          // P1: top-left
          {.x = 20, .y = 20},

          // P2: left, 1/3 down
          {.x = 20, .y = WINDOW_HEIGHT / 3},

          // P3: top-right
          {.x = WINDOW_WIDTH - 70, .y = 20},

          // P4: right, 1/3 down
          {.x = WINDOW_WIDTH - 70, .y = WINDOW_HEIGHT / 3},
      }};

  // This offers only a little extra protection if changes are made.
  _Static_assert(sizeof(preset_player_pos.pos) / sizeof(preset_player_pos.pos[0]) == 5,
                 "preset_player_pos.pos has wrong number of elements");

  for (int i = 0; i < MAX_PLAYERS; i++) {
    player[i] = (struct player_t){
        .name = "Testy", .id = -1, .pos = preset_player_pos.pos[i], .chips = 20000};
  }
}

int run_server(void) {
  struct player_t player[MAX_PLAYERS];
  init_players(player);

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

  int client_count = 0;
  while (client_count < MAX_CLIENTS) {
    TCPsocket new_client = SDLNet_TCP_Accept(server);
    if (new_client) {
      clients[client_count] = new_client;
      SDLNet_TCP_AddSocket(socket_set, new_client);

      IPaddress *remote_ip = SDLNet_TCP_GetPeerAddress(new_client);
      if (remote_ip) {
        Uint32 ipaddr = SDL_SwapBE32(remote_ip->host);
        Uint16 port = SDL_SwapBE16(remote_ip->port);
        printf("Client %d connected from %d.%d.%d.%d:%d\n", client_count, (ipaddr >> 24) & 0xFF,
               (ipaddr >> 16) & 0xFF, (ipaddr >> 8) & 0xFF, ipaddr & 0xFF, port);
      }

      // Deal hand
      for (int i = 0; i < 5; i++) {
        player[client_count].hand.card[i] = deck.card[i + (5 * client_count)];
      }

      // Serialize and send
      size_t size = 0;
      uint8_t *data = serialize_player(&player[client_count], &size);
      uint32_t size_net = htonl(size);

      if (send_all_tcp(new_client, &size_net, sizeof(size_net)) == -1 ||
          send_all_tcp(new_client, data, size) == -1) {
        fprintf(stderr, "Failed to send hand to client %d\n", client_count);
        SDLNet_TCP_Close(new_client);
        free(data);
        continue;
      }

      printf("Sent 5-card hand to player %d\n", client_count);
      client_count++;
    }

    SDL_Delay(100); // Prevent CPU hogging
  }

  // Cleanup
  for (int i = 0; i < client_count; i++) {
    SDLNet_TCP_Close(clients[i]);
  }
  SDLNet_TCP_Close(server);
  SDLNet_FreeSocketSet(socket_set);
  SDLNet_Quit();
  SDL_Quit();

  return 0;
}
