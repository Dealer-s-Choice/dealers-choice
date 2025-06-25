/*
 server.h
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

#ifndef __SERVER_H
#define __SERVER_H

#include "dc_config.h"
#include "net.h"
#include "types.h"

#define GAME_ARGS                                                                                  \
  ArgsBroadcastGameState_t *args, Player_t *players_array, DH_Deck *deck,                          \
      const uint8_t n_betting_rounds, const uint8_t n_draws, const uint8_t n_stud_new_cards

Config_t init_game_state(GameState_t *game_state, Path_t *path, CliArgs_t *cli_args);

RealHand_t deal_cards_to_players(GameState_t *game_state, DH_Deck *deck, const uint8_t game_type);

void game_five_card_draw(GAME_ARGS);

void game_five_card_stud(GAME_ARGS);

int run_server(CliArgs_t *cli_args, Path_t *path);

#endif
