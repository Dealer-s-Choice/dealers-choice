/*
 client.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "graphics.h"

int run_client(void) {
  if (SDL_Init(0) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL or SDL_net init failed: %s\n", SDLNet_GetError());
    return 1;
  }

  IPaddress server_ip;
  if (SDLNet_ResolveHost(&server_ip, "127.0.0.1", 61357) == -1) {
    fprintf(stderr, "Failed to resolve server: %s\n", SDLNet_GetError());
    SDLNet_Quit();
    SDL_Quit();
    return 1;
  }

  TCPsocket client = SDLNet_TCP_Open(&server_ip);
  if (!client) {
    fprintf(stderr, "Failed to open client socket: %s\n", SDLNet_GetError());
    SDLNet_Quit();
    SDL_Quit();
    return 1;
  }

  // Receive size first
  uint32_t size_net = 0;
  if (recv_all_tcp(client, &size_net, sizeof(size_net)) != 0) {
    // handle error
  }

  printf("before conversion: %d\n", size_net);
  size_t size = ntohl(size_net);
  printf("after conversion: %zu\n", size);

  uint8_t *data = malloc(size);
  if (!data) {
    perror("malloc");
    SDLNet_TCP_Close(client);
    SDLNet_Quit();
    SDL_Quit();
    return 1;
  }

  // Now receive the serialized player data
  if (recv_all_tcp(client, data, size) != 0) {
    // handle error
  }

  puts("received full player data");

  // Deserialize
  struct player_t player = deserialize_player(data, size);

  printf("Deserialized name: %s\n", player.name);
  printf("Deserialized chips: %d\n", player.chips);
  printf("Deserialized id: %d\n", player.id);
  printf("Deserialized pos x,y: %d, %d\n", player.pos.x, player.pos.y);
  for (int i = 0; i < HAND_SIZE; ++i) {
    printf("Card %d: face=%d, suit=%s\n", i + 1, player.hand.card[i].face_val,
           get_card_unicode_suit(player.hand.card[i]));
  }

  free(data);

  SDLNet_TCP_Close(client);
  SDLNet_Quit();
  SDL_Quit();

  struct sdl_context_t sdl_context;
  init_sdl_window(&sdl_context, "Net Poker");
  run_sdl_loop(sdl_context.renderer, &player);
  do_sdl_cleanup(&sdl_context);

  return 0;
}
