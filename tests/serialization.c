#include "00_test.h"

/* Round-trips every Player_t field the wire carries: identity, the four bool
 * flags, and the hand cards (face_val + suit). */
void test_player(void) {
  Player_t player = {0};
  player = (Player_t){.nick = "Foo",
                      .id = 1,
                      .coins = STARTING_N_COINS,
                      .in = true,
                      .winner = true,
                      .is_connected = true,
                      .is_admin = true};
  player.hand.card[0] = (DH_Card){.face_val = 10, .suit = 2};
  player.hand.card[1] = (DH_Card){.face_val = 13, .suit = 0};

  size_t size = 0;
  uint8_t *data = serialize_player(&player, &size);

  Player_t r = deserialize_player(data, size);

  free(data);

  assert(strcmp(r.nick, "Foo") == 0);
  assert(r.id == 1);
  assert(r.coins == STARTING_N_COINS);
  assert(r.in == true);
  assert(r.winner == true);
  assert(r.is_connected == true);
  assert(r.is_admin == true);
  assert(r.hand.card[0].face_val == 10 && r.hand.card[0].suit == 2);
  assert(r.hand.card[1].face_val == 13 && r.hand.card[1].suit == 0);
}

/* Round-trips every scalar GameState_t field plus two players with distinct
 * flags and cards, so a future field added to one side but not the other is
 * caught here. */
void test_game_state(void) {
  GameState_t game_state = (GameState_t){.pot = 500,
                                         .dealer_id = 2,
                                         .round_opener_id = 1,
                                         .at_menu = true,
                                         .player_count = 2,
                                         .raises_remaining = 3,
                                         .prev_bet_amount = 150,
                                         .winner_declared = true,
                                         .draw_phase = true,
                                         .player[0] = {.nick = "Foo",
                                                       .id = 0,
                                                       .coins = STARTING_N_COINS,
                                                       .in = true,
                                                       .is_admin = true},
                                         .player[1] = {.nick = "Bar",
                                                       .id = 1,
                                                       .coins = 42,
                                                       .winner = true}};
  game_state.player[1].hand.card[0] = (DH_Card){.face_val = 7, .suit = 3};

  uint32_t size = 0;
  uint8_t *data = serialize_game_state(&game_state, &size);

  GameState_t r = {0};
  bool ok = deserialize_game_state(data, size, &r);

  free(data);

  assert(ok);
  assert(r.pot == 500);
  assert(r.dealer_id == 2);
  assert(r.round_opener_id == 1);
  assert(r.at_menu == true);
  assert(r.player_count == 2);
  assert(r.raises_remaining == 3);
  assert(r.prev_bet_amount == 150);
  assert(r.winner_declared == true);
  assert(r.draw_phase == true);
  assert(strcmp(r.player[0].nick, "Foo") == 0);
  assert(r.player[0].id == 0);
  assert(r.player[0].coins == STARTING_N_COINS);
  assert(r.player[0].in && r.player[0].is_admin);
  assert(strcmp(r.player[1].nick, "Bar") == 0);
  assert(r.player[1].coins == 42);
  assert(r.player[1].winner == true);
  assert(r.player[1].hand.card[0].face_val == 7 && r.player[1].hand.card[0].suit == 3);
}

_MAIN_HEAD_

test_player();
test_game_state();

_MAIN_TAIL_
