#include "00_test.h"

_MAIN_HEAD_

Path_t path = {0};
get_data_dir(&path);

GameState_t game_state;

// The config isn't used in tests yet, so removing the variable assignment for now
// Config_t config = init_game_state(&game_state, &path);
init_game_state(&game_state, &path);

for (int i = 0; i < 3; i++) {
  game_state.player[i].id = i;
  game_state.player[i].in = true;
}

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

game_state.player[0].id = -1;

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
assert(turn->id == 1);

game_state.player[4].id = -1;

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 2);

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 1);

_MAIN_TAIL_
