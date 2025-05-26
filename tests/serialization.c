#include "00_test.h"

void test_player(void) {
  Player_t player = {0};
  player = (Player_t){.name = "Foo", .id = 1, .coins = 20000};

  size_t size = 0;
  uint8_t *data = serialize_player(&player, &size);

  fprintf(stderr, "before conversion: %zu\n", size);
  uint32_t size_net = htonl(size);

  size = ntohl(size_net);
  fprintf(stderr, "after conversion: %zu\n", size);

  Player_t player_receiver = deserialize_player(data, size);

  free(data);

  assert(strcmp(player_receiver.name, "Foo") == 0);
  assert(player_receiver.id == 1);
  assert(player_receiver.coins == 20000);
}

void test_game_state(void) {
  GameState_t game_state = {0};
  game_state = (GameState_t){.pot = 500,
                             .turn_id = 3,
                             .at_menu = true,
                             .total_bets_plus_raises = 623,
                             .player[0] = {
                                 .name = "Foo",
                                 .id = 0,
                                 .coins = 20000,
                                 .in = true,
                                 .total_paid = 50,
                             }};

  size_t size = 0;
  uint8_t *data = serialize_game_state(&game_state, &size);

  fprintf(stderr, "before conversion: %zu\n", size);
  uint32_t size_net = htonl(size);

  size = ntohl(size_net);
  fprintf(stderr, "after conversion: %zu\n", size);

  GameState_t game_state_receiver = deserialize_game_state(data, size);

  free(data);

  assert(game_state_receiver.pot == 500);
  assert(game_state_receiver.at_menu == true);
  assert(strcmp(game_state_receiver.player[0].name, "Foo") == 0);
  assert(game_state_receiver.total_bets_plus_raises == 623);
  assert(game_state_receiver.turn_id == 3);
  assert(game_state_receiver.player[0].id == 0);
  assert(game_state_receiver.player[0].coins == 20000);
  assert(game_state_receiver.player[0].in);
  assert(game_state_receiver.player[0].total_paid == 50);
}

_MAIN_HEAD_

test_player();
test_game_state();

_MAIN_TAIL_
