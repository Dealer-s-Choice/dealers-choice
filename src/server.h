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

void init_game_state(Game_State *game_state);

RealHand deal_cards_to_players(Game_State *game_state, struct player_t *dealer,
                               struct dh_deck *deck, const char game_type);

                               void game_five_card_draw(args_broadcast_game_state_t *args,
                                struct player_t *players_array,
                                struct player_t *dealer, struct dh_deck *deck);

void game_five_card_stud(args_broadcast_game_state_t *args,
                                struct player_t *players_array,
                                struct player_t *dealer, struct dh_deck *deck);

int run_server(void);

#endif
