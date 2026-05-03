/*
 tests/sodium_compat.c

 Verifies that the authentication handshake completes successfully.

 A minimal server is run in a thread: it sends a nonce and receives the
 hash response.  The client side calls authenticate_with_server(), the
 real production function.  The server side just drains the bytes —
 the test asserts only that the handshake completes without error,
 exercising the protocol framing.
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

  tcpme_socket_t server_sock = tcpme_listen("127.0.0.1", args->port);
  if (!tcpme_socket_valid(server_sock)) {
    fprintf(stderr, "server: listen failed: %s\n", tcpme_get_error());
    return -1;
  }

  tcpme_socket_t client = TCPME_INVALID_SOCKET;
  for (int i = 0; i < 50 && !tcpme_socket_valid(client); i++) {
    client = tcpme_accept(server_sock);
    SDL_Delay(50);
  }
  tcpme_close(server_sock);

  if (!tcpme_socket_valid(client)) {
    fprintf(stderr, "server: no client connected within timeout\n");
    return -1;
  }

  /* Send nonce — zeros are fine here; the server side only checks that
     HASH_SIZE bytes arrive, not whether they match. */
  unsigned char nonce[NONCE_SIZE];
  memset(nonce, 0, sizeof(nonce));
  if (send_all_tcp(client, nonce, NONCE_SIZE) != 0) {
    fprintf(stderr, "server: failed to send nonce\n");
    tcpme_close(client);
    return -1;
  }

  /* Receive the hash response — discard it; we only care that the client
     sends exactly HASH_SIZE bytes without hanging or erroring. */
  unsigned char hash[HASH_SIZE];
  int rc = recv_all_tcp(client, hash, HASH_SIZE);
  tcpme_close(client);

  args->result = (rc < 0) ? -1 : 0;
  return args->result;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  if (SDL_Init(0) != 0) {
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }
  if (tcpme_init() != 0) {
    fprintf(stderr, "tcpme_init failed: %s\n", tcpme_get_error());
    SDL_Quit();
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

  tcpme_socket_t sock = TCPME_INVALID_SOCKET;
  for (int i = 0; i < 20 && !tcpme_socket_valid(sock); i++) {
    sock = tcpme_connect("127.0.0.1", port);
    if (!tcpme_socket_valid(sock))
      SDL_Delay(100);
  }
  assert(tcpme_socket_valid(sock));

  int client_rc = authenticate_with_server(sock, "");
  tcpme_close(sock);

  int thread_rc;
  SDL_WaitThread(thread, &thread_rc);

  assert(client_rc == 0);
  assert(thread_rc == 0);
  assert(server_args.result == 0);

  fprintf(stderr, "sodium_compat: handshake OK (client=%d server=%d)\n", client_rc, thread_rc);

  tcpme_quit();
  SDL_Quit();
  return 0;
}
