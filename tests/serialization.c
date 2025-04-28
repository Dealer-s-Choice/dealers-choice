#include "00_test.h"

void test_player(void) {
  struct player_t player = {0};
  player = (struct player_t){.name = "Foo", .id = 1, .chips = 20000};

  size_t size = 0;
  uint8_t *data = serialize_player(&player, &size);

  fprintf(stderr, "before conversion: %zu\n", size);
  uint32_t size_net = htonl(size);

  size = ntohl(size_net);
  fprintf(stderr, "after conversion: %zu\n", size);

  struct player_t player_receiver = deserialize_player(data, size);

  free(data);

  assert(strcmp(player_receiver.name, "Foo") == 0);
  assert(player_receiver.id == 1);
  assert(player_receiver.chips == 20000);
}

void test_game_state(void) {
  struct game_state_t game_state = {0};
  game_state =
      (struct game_state_t){.pot = 500, .player[0] = {.name = "Foo", .id = 1, .chips = 20000}};

  size_t size = 0;
  uint8_t *data = serialize_game_state(&game_state, &size);

  fprintf(stderr, "before conversion: %zu\n", size);
  uint32_t size_net = htonl(size);

  size = ntohl(size_net);
  fprintf(stderr, "after conversion: %zu\n", size);

  struct game_state_t game_state_receiver = deserialize_game_state(data, size);

  free(data);

  assert(game_state_receiver.pot == 500);
  assert(strcmp(game_state_receiver.player[0].name, "Foo") == 0);
  assert(game_state_receiver.player[0].id == 1);
  assert(game_state_receiver.player[0].chips == 20000);
}

_MAIN_HEAD_

test_player();
test_game_state();

_MAIN_TAIL_
