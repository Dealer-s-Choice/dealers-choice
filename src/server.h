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

#include "net.h"
#include "types.h"

// These should be defined in a config file, which doesn't exist yet
#define ACTION_TIMEOUT_MS 20000
#define END_OF_ROUND_TIMEOUT_MS 15000

#define GAME_ARGS                                                                                  \
  ArgsBroadcastGameState_t *args, Player_t *players_array, Player_t *dealer, DH_Deck *deck,        \
      const uint8_t n_betting_rounds, const uint8_t draws

void init_game_state(GameState_t *game_state);

RealHand_t deal_cards_to_players(GameState_t *game_state, Player_t *dealer, DH_Deck *deck,
                                 const uint8_t game_type);

void game_five_card_draw(ArgsBroadcastGameState_t *args, Player_t *players_array, Player_t *dealer,
                         DH_Deck *deck, const uint8_t n_betting_rounds, const uint8_t draws);

void game_five_card_stud(GAME_ARGS);

int run_server(void);

#endif
