#include "00_test.h"

_MAIN_HEAD_

struct game_state_t game_state = {0};
init_game_state(&game_state);

for (int i = 0; i < 3; i++)
  game_state.player[i].id = i;

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

game_state.player[4].id = 4;
game_state.player[0].id = -1;

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

_MAIN_TAIL_
