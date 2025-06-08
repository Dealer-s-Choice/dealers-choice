int n_passes = 3;

char *ptr_n_passes = getenv("DC_NPASSES");
if (ptr_n_passes)
  n_passes = atoi(ptr_n_passes);
else if (argc > 1)
  n_passes = atoi(argv[1]);

GameState_t game_state[2] = {0};
ClientState_t client_state[2] = {0};
PlayerConfig_t player_config = get_player_config();

const bool test_mode = true;
SocketContext_t socket_context[2] = {0};
const int n_seconds = 1;
Path_t path = {0};

ERecvStatus_t recv_status;

for (int i = 0; i < 2; i++) {
  socket_context[i] =
      get_socket_context_and_run_client(&player_config, "127.0.0.1", NULL, NULL, &path, test_mode);
  assert(socket_context[i].sock != NULL);
  sleep(n_seconds);
}

for (int game = 0; game < n_passes; game++) {
  fprintf(stderr, "\n-#- game: %d\n", game);
  sleep(n_seconds);
  int i;

#include "_receive_game_state.c"
#include "_receive_game_state.c"

  int8_t *dealer_id = &game_state[0].dealer_id;
  const int expected_dealer_turn[3][3] = {{0, 0}, {1, 1}, {2, 0}};
  assert(expected_dealer_turn[game][1] == *dealer_id);
