// Regression test for: layout_cards infinite loop when slot-0 player disconnects
// https://github.com/Dealer-s-Choice/dealers-choice/issues/195
//
// In v0.0.9, layout_cards started its do-while from players_array[0] unconditionally.
// If slot 0 was disconnected and the remaining player was in a higher slot,
// get_next_connected_client never returned the disconnected starting_turn pointer,
// causing an infinite loop that froze the client window.

#include "00_test.h"

_MAIN_HEAD_

Player_t players_array[MAX_PLAYERS] = {0};
CardContext_t card_context[MAX_PLAYERS][MAX_HAND_SIZE] = {0};
SDL_Point player_pos[MAX_PLAYERS] = {0};

// Slot 0 is disconnected (the player who left)
players_array[0].id = 0;
players_array[0].is_connected = false;
players_array[0].in = false;

// Slot 1 is the remaining connected player
players_array[1].id = 1;
players_array[1].is_connected = true;
players_array[1].in = true;

// Give slot 1 a known position so we can verify the layout ran
player_pos[1].x = 100;
player_pos[1].y = 200;

// Before the fix this would loop forever; after the fix it returns immediately.
layout_cards(card_context, players_array, player_pos);

// Verify that slot 1's cards were laid out (rect.w is card_area.w = 80)
assert(card_context[1][0].rect.w > 0);
assert(card_context[1][0].rect.x >= player_pos[1].x);
assert(card_context[1][0].rect.y == player_pos[1].y);

// Slot 0 (disconnected) should not have been touched
assert(card_context[0][0].rect.w == 0);

_MAIN_TAIL_
