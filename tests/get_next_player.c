#include "00_test.h"

_MAIN_HEAD_

Path_t path = {0};
get_data_dir(&path);

GameState_t game_state;

// test_mode isn't needed here; it isn't used or required for all tests
bool test_mode = false;
init_game_state(&game_state, &path, test_mode);

for (int i = 0; i < 3; i++) {
  game_state.player[i].id = i;
  game_state.player[i].in = true;
}

game_state.player[3].in = true;
game_state.player[4].in = true;

game_state.player[4].id = 4;

assert(game_state.player[1].id == 1);

Player_t *players_array = game_state.player;
Player_t *turn = players_array;

assert(turn->id == 0);

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 1);

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 2);

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 4);

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 0);

game_state.player[0].id = 0;

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 1);

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 2);

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 4);

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 0);

game_state.player[4].id = 03;

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 1);

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 2);

_MAIN_TAIL_
