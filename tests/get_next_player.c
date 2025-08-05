#include "00_test.h"

_MAIN_HEAD_

Path_t path = {0};
get_data_dir(&path);

GameState_t game_state;

CliArgs_t cli_args = {0};
// test_mode isn't needed here; it isn't used or required for all tests
cli_args.test_mode = false;
init_game_state(&game_state, &path, &cli_args);

for (int i = 0; i < 3; i++) {
  game_state.player[i].is_connected = true;
  game_state.player[i].in = true;
}

game_state.player[3].in = true;
game_state.player[3].is_connected = false;
game_state.player[4].in = true;
game_state.player[4].is_connected = true;

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

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 1);

game_state.player[2].in = false;

game_state.player[3].in = true;
game_state.player[3].is_connected = true;

turn = get_next_player(players_array, turn->id);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 3);

turn = get_next_player(players_array, 2);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 3);

game_state.player[3].is_connected = false;

turn = get_next_player(players_array, 2);
fprintf(stderr, "turn->id: %d\n", turn->id);
assert(turn->id == 4);

for (int i = 1; i < MAX_PLAYERS; i++) {
  game_state.player[i].in = false;
}

game_state.player[0].in = true;

turn = get_next_player(players_array, 0);
assert(turn->id == 0);

// turn = get_next_player(players_array, turn->id);
// fprintf(stderr, "turn->id: %d\n", turn->id);
// assert(turn->id == 3);

_MAIN_TAIL_
