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
#include "game.h"
#include "graphics.h"

int run_client(const char *addr, struct sdl_context_t *sdl_context, struct font_t *font) {
  IPaddress server_ip;
  if (SDLNet_ResolveHost(&server_ip, addr, default_port) == -1) {
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

  int32_t net_player_id;
  uint32_t my_id;
  if (recv_all_tcp(client_socket, &net_player_id, sizeof(int32_t)) > 0) {
    my_id = ntohl(net_player_id);
    printf("Assigned id %d by server\n", my_id);
  } else {
    goto cleanup;
  }

  game_state_t game_state = {0};
  if (recv_game_state(client_socket, socket_set, &game_state) != 0)
    goto cleanup;

  run_sdl_loop(&game_state, sdl_context, font, client_socket, socket_set, my_id);

cleanup:
  SDLNet_TCP_DelSocket(socket_set, client_socket);
  SDLNet_FreeSocketSet(socket_set);
  SDLNet_TCP_Close(client_socket);
  SDLNet_Quit();
  return 0;
}
