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

int run_client(const char *addr) {
  IPaddress server_ip;
  if (SDLNet_ResolveHost(&server_ip, addr, 61357) == -1) {
    fprintf(stderr, "Failed to resolve server: %s\n", SDLNet_GetError());
    return 1;
  }

  TCPsocket client_socket = SDLNet_TCP_Open(&server_ip);
  if (!client_socket) {
    fprintf(stderr, "Failed to connect to server: %s\n", SDLNet_GetError());
    return 1;
  }

  SDLNet_SocketSet socket_set = SDLNet_AllocSocketSet(1);
  if (!socket_set) {
    fprintf(stderr, "Failed to allocate socket set: %s\n", SDLNet_GetError());
    SDLNet_TCP_Close(client_socket);
    return 1;
  }

  if (SDLNet_TCP_AddSocket(socket_set, client_socket) == -1)
    fputs("Socket set full\n", stderr);

  // Receive initial game state
  struct game_state_t game_state = {0};
  uint32_t size_net = 0;
  if (SDLNet_TCP_Recv(client_socket, &size_net, sizeof(size_net)) != sizeof(size_net)) {
    fprintf(stderr, "Failed to receive game state size\n");
    goto cleanup;
  }

  uint32_t size = ntohl(size_net);
  uint8_t *buffer = malloc(size);
  if (!buffer) {
    fprintf(stderr, "Out of memory\n");
    goto cleanup;
  }

  size_t received = 0;
  while (received < size) {
    int r = SDLNet_TCP_Recv(client_socket, buffer + received, size - received);
    if (r <= 0) {
      fprintf(stderr, "Connection lost while receiving\n");
      free(buffer);
      goto cleanup;
    }
    received += r;
  }

  game_state = deserialize_game_state(buffer, size);
  free(buffer);

  run_sdl_loop(&game_state, client_socket, socket_set);

cleanup:
  SDLNet_TCP_DelSocket(socket_set, client_socket);
  SDLNet_FreeSocketSet(socket_set);
  SDLNet_TCP_Close(client_socket);
  SDLNet_Quit();
  SDL_Quit();
  return 0;
}
