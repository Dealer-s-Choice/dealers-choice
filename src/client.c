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

SocketContext_t run_client(const char *addr, SdlContext_t *sdl_context, Font_t *font,
                           const bool test_mode) {
  IPaddress server_ip;
  SocketContext_t socket_context = {NULL, NULL, -1};
  if (SDLNet_ResolveHost(&server_ip, addr, default_port) == -1) {
    fprintf(stderr, "Failed to resolve server: %s\n", SDLNet_GetError());
    return socket_context;
  }

  socket_context.sock = SDLNet_TCP_Open(&server_ip);
  if (!socket_context.sock) {
    fprintf(stderr, "Failed to connect to server: %s\n", SDLNet_GetError());
    return socket_context;
    ;
  }

  socket_context.set = SDLNet_AllocSocketSet(1);
  if (!socket_context.set) {
    fprintf(stderr, "Failed to allocate socket set: %s\n", SDLNet_GetError());
    SDLNet_TCP_Close(socket_context.sock);
    return socket_context;
  }

  if (SDLNet_TCP_AddSocket(socket_context.set, socket_context.sock) == -1)
    fputs("Socket set full\n", stderr);

  int32_t net_player_id;
  if (recv_all_tcp(socket_context.sock, &net_player_id, sizeof(int32_t)) > 0) {
    socket_context.id = ntohl(net_player_id);
    printf("Assigned id %d by server\n", socket_context.id);
  } else {
    goto cleanup;
  }

  GameState_t game_state = {0};
  ClientState_t client_state = {0};
  if (recv_game_state(socket_context.sock, socket_context.set, &game_state, &client_state,
                      socket_context.id) != RECV_SUCCESS)
    goto cleanup;

  if (!test_mode)
    run_sdl_loop(&game_state, &client_state, sdl_context, font, socket_context.sock,
                 socket_context.set, socket_context.id);
  else
    return socket_context;

cleanup:
  SDLNet_TCP_DelSocket(socket_context.set, socket_context.sock);
  SDLNet_FreeSocketSet(socket_context.set);
  SDLNet_TCP_Close(socket_context.sock);
  SDLNet_Quit();
  return socket_context;
}
