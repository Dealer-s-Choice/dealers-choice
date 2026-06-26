/*
 tests/identity_handshake.c

 Exercises the identity challenge-response (#67) over a real loopback socket,
 end to end: a server thread runs identity_handshake_server() (the production
 function) while the main thread connects and runs identity_handshake_client().
 Asserts both sides succeed AND that the public key the server authenticated
 matches the client's actual identity. The crypto-level rejection paths (bad
 signature / wrong key) are covered by the unit test in tests/identity.c.
*/

#include <string.h>

#include "00_test.h"

#include "dc_identity.h"

#define TEST_KEY "dc_id_handshake_test.key"

typedef struct {
  uint16_t port;
  int result;                                       /* 0 on success */
  unsigned char pubkey[crypto_sign_PUBLICKEYBYTES]; /* what the server authenticated */
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

  args->result = identity_handshake_server(client, args->pubkey);
  tcpme_close(client);
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

  /* Use a throwaway identity at a local path (not the default), so the test is
     hermetic and independent of the user's real identity file. */
  remove(TEST_KEY);
  DcIdentity_t id = {0};
  assert(dc_identity_load_or_create(TEST_KEY, &id));

  uint16_t port = 22789;
  const char *port_env = getenv("DC_PORT");
  if (port_env) {
    unsigned long v = strtoul(port_env, NULL, 10);
    if (v > 0 && v <= 65535)
      port = (uint16_t)v;
  }

  ServerArgs_t server_args = {.port = port, .result = -1};
  SDL_Thread *thread = SDL_CreateThread(server_thread, "id_hs_server", &server_args);
  assert(thread != NULL);

  SDL_Delay(200);

  tcpme_socket_t sock = TCPME_INVALID_SOCKET;
  for (int i = 0; i < 20 && !tcpme_socket_valid(sock); i++) {
    sock = tcpme_connect("127.0.0.1", port);
    if (!tcpme_socket_valid(sock))
      SDL_Delay(100);
  }
  assert(tcpme_socket_valid(sock));

  int client_rc = identity_handshake_client(sock, &id);
  tcpme_close(sock);

  int thread_rc;
  SDL_WaitThread(thread, &thread_rc);

  assert(client_rc == 0);
  assert(thread_rc == 0);
  assert(server_args.result == 0);
  /* The key the server authenticated must be the client's real public key. */
  assert(memcmp(server_args.pubkey, id.public_key, sizeof(id.public_key)) == 0);

  fprintf(stderr, "identity_handshake: OK (client=%d server=%d, pubkey matched)\n", client_rc,
          thread_rc);

  remove(TEST_KEY);
  tcpme_quit();
  SDL_Quit();
  return 0;
}
