/*
 game.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025 Andy Alt

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "game.h"

const GameChoice_t game_choices[] = {
    {FIVE_CARD_DRAW, "5-card draw", 0x01, game_five_card_draw, 2, 1, 0},
    {FIVE_CARD_DOUBLE_DRAW, "5-card double draw", 0x02, game_five_card_draw, 3, 2, 0},
    {FIVE_CARD_STUD, "5-card stud", 0x03, game_five_card_stud, 4, 0, 3},
    {FIVE_CARD_SHOWDOWN, "5-card showdown", 0x04, game_five_card_draw, 1, 0, 0}};

const GameChoice_t *find_game_choice_by_type(const uint8_t type) {
  for (size_t i = 0; i < sizeof(game_choices) / sizeof(game_choices[0]); ++i) {
    if (game_choices[i].game_type == type) {
      return &game_choices[i];
    }
  }
  return NULL; // Not found
}

static bool is_valid_player(const Player_t *p, bool want_all_clients) {
  return p->id != -1 && (want_all_clients || p->in);
}

static Player_t *get_next_player_real(Player_t *players_array, int cur, bool want_all_clients) {
  if (cur < 0) {
    fprintf(stderr, "%s: 'cur' may not be a negative value.\n", __func__);
    exit(EXIT_FAILURE);
  }

  int i = (cur + 1) % MAX_PLAYERS;

  while (i != cur) {
    // fprintf(stderr, "i: %d\n", i);
    if (is_valid_player(&players_array[i], want_all_clients)) {
      return &players_array[i];
    }
    i = (i + 1) % MAX_PLAYERS;
  }

  // fprintf(stderr, "i: %d\n", i);
  // Final fallback: check starting index
  if (is_valid_player(&players_array[cur], want_all_clients)) {
    return &players_array[cur];
  }

  fputs("No valid players found\n", stderr);
  exit(EXIT_FAILURE);
}

Player_t *get_next_player(Player_t *players_array, int cur) {
  const bool want_all_clients = false;
  return get_next_player_real(players_array, cur, want_all_clients);
}

Player_t *get_next_connected_client(Player_t *players_array, int cur) {
  const bool want_all_clients = true;
  return get_next_player_real(players_array, cur, want_all_clients);
}

CliArgs_t init_cli_args(void) {
  CliArgs_t cli_args = {
      .host = NULL,
      .server_conf = NULL,
      .server_log_game_results_file = NULL,
      .bind_address = NULL,
      .test_mode = false,
      .run_server_flag = false,
  };
  return cli_args;
}

pcg32_random_t rng;
void pcg_srand_auto(void) {
  uint64_t initstate = time(NULL) ^ (intptr_t)&printf;
  uint64_t initseq = (intptr_t)&pcg_srand_auto;
  pcg32_srandom_r(&rng, initstate, initseq);
}
