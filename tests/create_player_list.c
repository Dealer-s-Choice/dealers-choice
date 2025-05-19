#include "00_test.h"

_MAIN_HEAD_

game_state_t game_state;
init_game_state(&game_state);

for (int i = 0; i < 3; i++) {
  game_state.player[i].id = i;
  game_state.player[i].in = true;
}

struct player_list_t *active_players = create_player_list(&game_state);
assert(active_players);

struct player_list_t *root = active_players;

int i = 0;
do {
  // fprintf(stderr, "id: %d\n", active_players->id);
  assert(active_players->id == i++);
  active_players = active_players->next;
} while (active_players != root);

free_player_list(root);
assert(game_state.player_count == 3);
game_state.player_count = 0;

game_state.player[4].id = 4;
game_state.player[4].in = 4;

game_state.player[0].in = false;

active_players = create_player_list(&game_state);
assert(active_players);
root = active_players;

int res[] = {1, 2, 4, 1};

for (i = 0; (size_t)i < sizeof res / sizeof res[0]; i++) {
  fprintf(stderr, "id: %d\n", active_players->id);
  assert(active_players->id == res[i]);
  active_players = active_players->next;
}

free_player_list(root);

assert(game_state.player_count == 3);

_MAIN_TAIL_
