/*
 tests/sodium_compat.c

 Verifies that the authentication handshake completes successfully
 regardless of whether libsodium is available at compile time.

 A minimal server is run in a thread: it sends a nonce and receives the
 hash response.  The client side calls authenticate_with_server(), the
 real production function.  When sodium is present the client sends a
 real SHA-256 hash; when absent it sends zeros.  Either way the server
 side just drains the bytes and accepts — the test asserts only that the
 handshake completes without error, exercising the protocol framing.
*/

#include <string.h>

#include "00_test.h"

typedef struct {
  uint16_t port;
  int result;
} ServerArgs_t;

static int SDLCALL server_thread(void *data) {
  ServerArgs_t *args = (ServerArgs_t *)data;
  args->result = -1;

  IPaddress ip;
  if (SDLNet_ResolveHost(&ip, NULL, args->port) != 0) {
    fprintf(stderr, "server: ResolveHost failed: %s\n", SDLNet_GetError());
    return -1;
  }

  TCPsocket server_sock = SDLNet_TCP_Open(&ip);
  if (!server_sock) {
    fprintf(stderr, "server: TCP_Open failed: %s\n", SDLNet_GetError());
    return -1;
  }

  TCPsocket client = NULL;
  for (int i = 0; i < 50 && !client; i++) {
    client = SDLNet_TCP_Accept(server_sock);
    SDL_Delay(50);
  }
  SDLNet_TCP_Close(server_sock);

  if (!client) {
    fprintf(stderr, "server: no client connected within timeout\n");
    return -1;
  }

  /* Send nonce — use zeros to match the sodium-absent fallback, so the
     test is symmetric and doesn't depend on crypto being available. */
  unsigned char nonce[NONCE_SIZE];
  memset(nonce, 0, sizeof(nonce));
  if (send_all_tcp(client, nonce, NONCE_SIZE) != 0) {
    fprintf(stderr, "server: failed to send nonce\n");
    SDLNet_TCP_Close(client);
    return -1;
  }

  /* Receive the hash response — discard it; we only care that the client
     sends exactly HASH_SIZE bytes without hanging or erroring. */
  unsigned char hash[HASH_SIZE];
  int rc = recv_all_tcp(client, hash, HASH_SIZE);
  SDLNet_TCP_Close(client);

  args->result = (rc < 0) ? -1 : 0;
  return args->result;
}

int main(void) {
  if (SDL_Init(0) != 0 || SDLNet_Init() != 0) {
    fprintf(stderr, "SDL/SDLNet init failed: %s\n", SDL_GetError());
    return 1;
  }

  uint16_t port = 22784;
  const char *port_env = getenv("DC_PORT");
  if (port_env) {
    unsigned long v = strtoul(port_env, NULL, 10);
    if (v > 0 && v <= 65535)
      port = (uint16_t)v;
  }

  ServerArgs_t server_args = {.port = port, .result = -1};
  SDL_Thread *thread = SDL_CreateThread(server_thread, "auth_server", &server_args);
  assert(thread != NULL);

  SDL_Delay(200);

  IPaddress ip;
  assert(SDLNet_ResolveHost(&ip, "127.0.0.1", port) == 0);

  TCPsocket sock = NULL;
  for (int i = 0; i < 20 && !sock; i++) {
    sock = SDLNet_TCP_Open(&ip);
    if (!sock)
      SDL_Delay(100);
  }
  assert(sock != NULL);

  int client_rc = authenticate_with_server(sock, "");
  SDLNet_TCP_Close(sock);

  int thread_rc;
  SDL_WaitThread(thread, &thread_rc);

  assert(client_rc == 0);
  assert(thread_rc == 0);
  assert(server_args.result == 0);

  fprintf(stderr, "sodium_compat: handshake OK (client=%d server=%d)\n", client_rc,
          thread_rc);

  SDLNet_Quit();
  SDL_Quit();
  return 0;
}
