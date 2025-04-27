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
#include <string.h>

#include "graphics.h"
#include "client.h"
#include "netpoker.pb-c.h"

static void assign_tcp_dual_stack_client_fd(struct socket_info_t *socket_info) {
  struct addrinfo hints, *res, *p;

  // Set up hints for getaddrinfo()
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; // Allow both IPv4 and IPv6
  hints.ai_socktype = SOCK_STREAM;

  // Get address info
  if (getaddrinfo(socket_info->host, socket_info->port, &hints, &res) != 0) {
    perror("getaddrinfo");
    exit(EXIT_FAILURE);
  }

  // Try to connect to one of the results
  for (p = res; p != NULL; p = p->ai_next) {
    socket_info->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (socket_info->sockfd == INVALID_SOCKET)
      continue;

    if (connect(socket_info->sockfd, p->ai_addr, p->ai_addrlen) == 0)
      break; // Connected successfully

    close_socket_checked(socket_info->sockfd);
  }

  freeaddrinfo(res);

  if (!p) {
    perror("Failed to connect");
    exit(EXIT_FAILURE);
  }
  return;
}

int run_client(void) {

#ifdef _WIN32
  WSADATA wsaData;
  int iResult;

  // Initialize Winsock version 2.2
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    printf("WSAStartup failed: %d\n", iResult);
    return 1;
  }
#endif

  struct socket_info_t socket_info = {
      .port = default_port,
      .host = "127.0.0.1",
  };
  assign_tcp_dual_stack_client_fd(&socket_info);

  uint32_t size_net = 0;
  if (recv_all(socket_info.sockfd, &size_net, sizeof(size_net)) != 0) {
    perror("recv size");
    exit(1);
  }

  printf("before conversion: %d\n", size_net);
  size_t size = ntohl(size_net);
  printf("after conversion: %zd\n", size);

  uint8_t *data = malloc(size);
  if (!data) {
    perror("malloc");
    exit(1);
  }

  ssize_t received = recv(socket_info.sockfd, (void *)data, size, 0);
  if (received <= 0) {
    perror("recv");
    free(data);
    exit(1);
  } else
    puts("received data");

  // Deserialize
  struct player_t player = deserialize_player(data, size);

  printf("Deserialized name: %s\n", player.name);
  printf("Deserialized chips: %d\n", player.chips);
  printf("Deserialized id: %d\n", player.id);
  printf("Deserialized pos x,y: %d, %d\n", player.pos.x, player.pos.y);
  int i;
  for (i = 0; i < HAND_SIZE; ++i) {
    printf("Card %d: face=%d, suit=%s\n", i + 1, player.hand.card[i].face_val,
           get_card_unicode_suit(player.hand.card[i]));
  }

  free(data);

  if (socket_info.sockfd != INVALID_SOCKET)
    close_socket_checked(socket_info.sockfd);

#ifdef _WIN32
  WSACleanup();
#endif

  struct sdl_context_t sdl_context;
  init_sdl_window(&sdl_context, "Net Poker");
  run_sdl_loop(sdl_context.renderer, &player);
  do_sdl_cleanup(&sdl_context);

  return 0;
}
