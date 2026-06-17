/*
 games.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2026 Andy Alt

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

/* The poker game variants and the per-hand play orchestration (play_game).
 * Split out of server.c; uses the betting engine, broadcast, and showdown
 * helpers declared in server_internal.h. */

#include <stdio.h>
#include <string.h>

#include "dc_time.h"
#include "game.h"
#include "server.h"
#include "server_internal.h"
#include "util.h"

void game_five_card_draw(ArgsBroadcastGameState_t *args, Player_t *players_array, DH_Deck *deck,
 const GameChoice_t *choice) {
  server_handle_ante(args->game_state, args->config->ante);

  Player_t *turn = *args->starting_turn;
  do {
    handle_sort_hand(&args->real_hand[turn->id],
                     args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type,
                     args->deuces_wild);
    turn = get_next_player(players_array, turn->id);
  } while (turn && turn != *args->starting_turn);

  RoundResults results = {0};

  for (int i = 0; i < choice->n_betting_rounds; i++) {
    results = handle_round_real(args, 0, -1);
    if (results.n_winners > 0 || i == choice->n_draws)
      break;

    broadcast_game_state(args);
    turn = *args->starting_turn;

    do {
      args->turn_id = turn->id;
      verbose_printf("turn->id: %d\n", turn->id);

      broadcast_turn_id(args);

      ELoop_t d = handle_draw(args, args->clients[turn->id], turn->id, deck);
      if (d == LOOP_BREAK) {
        // broadcast_game_state(args);
        if (args->game_state->player_count == 1) {
          award_last_player_in_game(args, turn, &results);
          break;
        }
      } else if (d != LOOP_OK)
        printf("Failed to receive cards or player disconnected: %d\n", turn->id);

      turn = get_next_player(players_array, turn->id);
    } while (turn && turn != *args->starting_turn);
    broadcast_game_state(args);
    if (results.n_winners > 0)
      break;
  }
  determine_winner(args, &results);
}

// Returns the index of the first face-up card within the initial deal, or -1 if none.
static int stud_upcard_idx(const GameChoice_t *choice) {
  for (int i = 0; i < choice->n_cards_initial_deal; i++)
    if (choice->card_slot[i] == CARD_SLOT_FACE_UP)
      return i;
  return -1;
}

// Returns the player whose upcard at real_hand[][upcard_idx] is lowest (bring-in player).
static Player_t *stud_find_bringin_player(const ArgsBroadcastGameState_t *args,
                                          Player_t *players_array, int upcard_idx) {
  Player_t *bringin = NULL;
  DH_Card bringin_card = DH_card_null;

  Player_t *turn = *args->starting_turn;
  do {
    if (!turn->in || !turn->is_connected) {
      turn = get_next_player(players_array, turn->id);
      continue;
    }
    DH_Card upcard = args->real_hand[turn->id].card[upcard_idx];
    if (DH_is_card_null(upcard)) {
      turn = get_next_player(players_array, turn->id);
      continue;
    }
    if (bringin == NULL || POKEVAL_card_bringin_lt(upcard, bringin_card)) {
      bringin = turn;
      bringin_card = upcard;
    }
    turn = get_next_player(players_array, turn->id);
  } while (turn && turn != *args->starting_turn);

  return bringin;
}

// Returns the player with the best visible upcard hand at a given street index.
// street_idx is the loop variable i (deal index about to be made); visible cards are
// 0..street_idx-1 where face_up[j] == true.
static Player_t *stud_find_best_upcard_player(const ArgsBroadcastGameState_t *args,
                                              Player_t *players_array, const GameChoice_t *choice,
                                              uint8_t street_idx) {
  Player_t *best_player = NULL;
  uint64_t best_score = 0;

  Player_t *turn = *args->starting_turn;
  do {
    if (!turn->in || !turn->is_connected) {
      turn = get_next_player(players_array, turn->id);
      continue;
    }

    // Collect this player's visible (face-up) cards dealt so far
    DH_Card visible[4];
    int n_visible = 0;
    for (int j = 0; j < street_idx && n_visible < 4; j++) {
      if (choice->card_slot[j] == CARD_SLOT_FACE_UP)
        visible[n_visible++] = args->real_hand[turn->id].card[j];
    }

    uint64_t score = POKEVAL_score_stud_upcards(visible, n_visible);
    if (best_player == NULL || score > best_score) {
      best_score = score;
      best_player = turn;
    }

    turn = get_next_player(players_array, turn->id);
  } while (turn && turn != *args->starting_turn);

  return best_player;
}

void game_stud(ArgsBroadcastGameState_t *args, Player_t *players_array, DH_Deck *deck,
 const GameChoice_t *choice) {
  Player_t *turn;
  server_handle_ante(args->game_state, args->config->ante);

  RoundResults results = {0};
  bool first_round = true;

  // Find the one face-up card in the initial deal — used to determine the bring-in player.
  int upcard_idx = stud_upcard_idx(choice);

  for (uint8_t i = choice->n_cards_initial_deal; i <= choice->hand_size; i++) {
    if (first_round) {
      // Bring-in: player with the lowest upcard posts a forced partial bet.
      Player_t *bringin =
          (upcard_idx >= 0) ? stud_find_bringin_player(args, players_array, upcard_idx) : NULL;

      if (bringin && args->config->bringin_amount > 0) {
        args->turn_id = bringin->id;
        bringin->coins -= (int32_t)args->config->bringin_amount;
        args->game_state->pot += args->config->bringin_amount;

        char bringin_str[LEN_STATUS_STR] = {0};
        snprintf(bringin_str, sizeof(bringin_str), _("%s brings in for %u"), bringin->nick,
                 args->config->bringin_amount);
        broadcast_status_message(args, bringin_str);

        // Action starts with the player to the left of the bring-in player.
        *args->starting_turn = get_next_player(players_array, bringin->id);
        broadcast_game_state(args);
        results = handle_round_real(args, args->config->bringin_amount, bringin->id);
      } else {
        results = handle_round_real(args, 0, -1);
      }
      first_round = false;
    } else {
      // Subsequent streets: player with the best visible hand acts first.
      Player_t *best = stud_find_best_upcard_player(args, players_array, choice, i);
      if (best)
        *args->starting_turn = best;
      results = handle_round_real(args, 0, -1);
    }

    if (results.n_winners > 0 || i == choice->hand_size)
      break;

    turn = *args->starting_turn;

    verbose_printf("round: %d\n", i);
    do {
      int id = turn->id;
      POKEVAL_Hand_9 *hand = &turn->hand;

      args->real_hand[id].card[i] = DH_deal_top_card(deck);
      if (choice->card_slot[i] == CARD_SLOT_FACE_UP)
        hand->card[i] = args->real_hand[id].card[i];
      else
        hand->card[i] = DH_card_back;
      turn = get_next_player(players_array, turn->id);
    } while (turn && turn != *args->starting_turn);
    broadcast_game_state(args);
  }

  determine_winner(args, &results);
}

static void deal_community_cards(ArgsBroadcastGameState_t *args, Player_t *players_array,
                                 DH_Deck *deck, uint8_t start_pos, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    uint8_t pos = start_pos + i;
    DH_Card card = DH_deal_top_card(deck);
    for (int p = 0; p < MAX_PLAYERS; p++) {
      if (!players_array[p].is_connected)
        continue;
      args->real_hand[p].card[pos] = card;
      players_array[p].hand.card[pos] = card;
    }
  }
}

void game_texas_holdem(ArgsBroadcastGameState_t *args, Player_t *players_array, DH_Deck *deck,
 const GameChoice_t *choice) {
  (void)choice;
  server_handle_ante(args->game_state, args->config->ante);

  RoundResults results = {0};

  /* Pre-flop betting (2 hole cards already dealt by play_game) */
  results = handle_round_real(args, 0, -1);
  if (results.n_winners > 0)
    goto done;

  /* Flop (3 cards), Turn (1), River (1) */
  const uint8_t street_start[] = {2, 5, 6};
  const uint8_t street_count[] = {3, 1, 1};
  for (size_t s = 0; s < ARRAY_SIZE(street_start); s++) {
    deal_community_cards(args, players_array, deck, street_start[s], street_count[s]);
    broadcast_game_state(args);
    results = handle_round_real(args, 0, -1);
    if (results.n_winners > 0)
      goto done;
  }

done:
  determine_winner(args, &results);
}

void game_omaha(ArgsBroadcastGameState_t *args, Player_t *players_array, DH_Deck *deck,
 const GameChoice_t *choice) {
  (void)choice;
  server_handle_ante(args->game_state, args->config->ante);

  RoundResults results = {0};

  /* Pre-flop betting (4 hole cards already dealt by play_game) */
  results = handle_round_real(args, 0, -1);
  if (results.n_winners > 0)
    goto done;

  /* Flop (3 cards), Turn (1), River (1) — community cards start at position 4 */
  const uint8_t street_start[] = {4, 7, 8};
  const uint8_t street_count[] = {3, 1, 1};
  for (size_t s = 0; s < ARRAY_SIZE(street_start); s++) {
    deal_community_cards(args, players_array, deck, street_start[s], street_count[s]);
    broadcast_game_state(args);
    results = handle_round_real(args, 0, -1);
    if (results.n_winners > 0)
      goto done;
  }

done:
  determine_winner(args, &results);
}

void game_seven_card_no_peek(ArgsBroadcastGameState_t *args, Player_t *players_array, DH_Deck *deck,
 const GameChoice_t *choice) {
  (void)deck;
  (void)choice;
  server_handle_ante(args->game_state, args->config->ante);

  RoundResults results = {0};
  memset(args->no_peek_n_flipped, 0, sizeof(args->no_peek_n_flipped));

  // All 7 cards are already dealt face-down. The player left of the dealer
  // flips their first card and opens the first betting round.
  Player_t *first = *args->starting_turn;
  args->no_peek_n_flipped[first->id] = 1;
  broadcast_game_state(args);

  uint64_t best_score = POKEVAL_score_visible_cards(&args->real_hand[first->id].card[0], 1);

  results = handle_round_real(args, 0, -1);
  if (results.n_winners > 0) {
    determine_winner(args, &results);
    return;
  }

  Player_t *current_best = first;
  Player_t *next_turn = get_next_player(players_array, first->id);

  // Each active player in clockwise order must flip cards until their visible
  // hand beats the current best showing, or they run out of cards and drop out.
  // This continues around the table (multiple passes) until only one player
  // remains or all active players have all 7 cards face up.
  while (args->game_state->player_count > 1) {
    if (!next_turn->in || !next_turn->is_connected) {
      next_turn = get_next_player(players_array, next_turn->id);
      continue;
    }

    bool beat = false;
    int *nf = &args->no_peek_n_flipped[next_turn->id];

    if (*nf >= 7) {
      // All cards already face up from a prior pass; compare current score.
      uint64_t score = POKEVAL_score_visible_cards(args->real_hand[next_turn->id].card, 7);
      if (score > best_score) {
        best_score = score;
        beat = true;
      }
    } else {
      // Flip cards one at a time until beating best_score or running out.
      while (*nf < 7) {
        (*nf)++;
        broadcast_game_state(args);

        uint64_t score = POKEVAL_score_visible_cards(args->real_hand[next_turn->id].card, *nf);

        if (score > best_score) {
          best_score = score;
          beat = true;
          break;
        }
      }
    }

    if (beat) {
      current_best = next_turn;
      *args->starting_turn = current_best;
      results = handle_round_real(args, 0, -1);
      if (results.n_winners > 0)
        break;
      next_turn = get_next_player(players_array, current_best->id);
    } else {
      // Ran out of cards without beating the current best — forced drop out.
      Player_t *out = next_turn;
      next_turn = get_next_player(players_array, out->id);

      out->in = false;
      args->no_peek_n_flipped[out->id] = 0;
      for (int i = 0; i < MAX_HAND_SIZE; i++) {
        args->real_hand[out->id].card[i] = DH_card_null;
        out->hand.card[i] = DH_card_null;
      }
      args->game_state->player_count--;
      if (out == *args->starting_turn)
        *args->starting_turn = get_next_player(players_array, out->id);

      char status_str[LEN_STATUS_STR] = {0};
      snprintf(status_str, sizeof status_str, _("%s ran out of cards"), out->nick);
      broadcast_status_message(args, status_str);

      if (args->game_state->player_count == 1) {
        award_last_player_in_game(args, current_best, &results);
        break;
      }
    }
  }

  determine_winner(args, &results);
}

void play_game(ArgsBroadcastGameState_t *args, DH_Deck *deck) {
  DH_shuffle_deck(deck);
  if (!args->cli_args->test_mode) {
    int cut_point = 16 + pcg32_boundedrand_r(&rng, 21);
    DH_cut_deck(deck, cut_point);
  }

  Player_t *players_array = args->game_state->player;
  memset(args->real_hand, 0, sizeof(args->real_hand));
  deal_cards_to_players(args->game_state, deck, args->game_type, args->real_hand);

  if (args->cli_args->test_mode) {
    static int test_case = 0;
    test_case++;
    if (test_case == 1) {
      for (int i = 1; i < 3; i++)
        for (int j = 0; j < 4; j++)
          args->real_hand[i].card[j].face_val = DH_CARD_ACE;

      args->real_hand[1].card[4].face_val = DH_CARD_KING;
      args->real_hand[2].card[4].face_val = DH_CARD_KING;
    } else if (test_case == 2) {
      for (int j = 0; j < 4; j++)
        args->real_hand[2].card[j].face_val = DH_CARD_ACE;
    } else if (test_case == 3) {
      for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
          args->real_hand[i].card[j].face_val = DH_CARD_ACE;

      args->real_hand[0].card[4].face_val = DH_CARD_KING;
      args->real_hand[1].card[4].face_val = DH_CARD_KING;
      args->real_hand[2].card[4].face_val = DH_CARD_KING;
    }
  }

  // args->real_hand[0].card[0].face_val = DH_CARD_TWO;
  // args->real_hand[0].card[3].face_val = DH_CARD_TWO;

  // args->real_hand[0].card[3].face_val = DH_CARD_TWO;
  // args->real_hand[1].card[3].face_val = DH_CARD_TWO;
  // args->real_hand[2].card[3].face_val = DH_CARD_TWO;

  // Lowball setups
  //
  // args->real_hand[0].card[0].face_val = DH_CARD_ACE;
  // args->real_hand[0].card[1].face_val = DH_CARD_TWO;
  // args->real_hand[0].card[2].face_val = DH_CARD_THREE;
  // args->real_hand[0].card[3].face_val = DH_CARD_FOUR;
  // args->real_hand[0].card[4].face_val = DH_CARD_SIX;

  // args->real_hand[1].card[0].face_val = DH_CARD_TWO;
  // args->real_hand[1].card[1].face_val = DH_CARD_THREE;
  // args->real_hand[1].card[2].face_val = DH_CARD_FOUR;
  // args->real_hand[1].card[3].face_val = DH_CARD_FIVE;
  // args->real_hand[1].card[4].face_val = DH_CARD_SIX;
  //
  //  In lowball, 8-5-4-3-2 defeats 9-7-6-4-3
  // args->real_hand[0].card[0].face_val = DH_CARD_EIGHT;
  // args->real_hand[0].card[1].face_val = DH_CARD_FIVE;
  // args->real_hand[0].card[2].face_val = DH_CARD_FOUR;
  // args->real_hand[0].card[3].face_val = DH_CARD_THREE;
  // args->real_hand[0].card[4].face_val = DH_CARD_TWO;

  // args->real_hand[1].card[0].face_val = DH_CARD_NINE;
  // args->real_hand[1].card[1].face_val = DH_CARD_SEVEN;
  // args->real_hand[1].card[2].face_val = DH_CARD_SIX;
  // args->real_hand[1].card[3].face_val = DH_CARD_FOUR;
  // args->real_hand[1].card[4].face_val = DH_CARD_THREE;

  args->game_state->winner_declared = false;
  args->game_state->prev_bet_amount = 0;
  args->game_state->round_opener_id = -1;
  args->game_state->player_count = count_active_clients(args->slot_taken);
  verbose_printf("player count: %d\n", args->game_state->player_count);
  args->game_state->winner_declared = false;

  Player_t *turn = get_next_player(players_array, args->game_state->dealer_id);
  args->starting_turn = &turn;
  broadcast_game_state(args);
  broadcast_game_type(args);

  const GameChoice_t *choice = find_game_choice_by_type(args->game_type);

  if (args->cli_args->server_log_game_results_file) {
    char tmp[LEN_STATUS_STR] = {0};
    snprintf(tmp, sizeof(tmp), _("Game: %s%s"), choice->str,
             args->deuces_wild ? " / Deuces Wild" : "");
    FILE *fp = fopen(args->cli_args->server_log_game_results_file, "a");
    if (!fp)
      perror("fopen");
    else {
      fprintf(fp, "### %s\n\n", tmp);
      Player_t *p = *args->starting_turn;
      do {
        fprintf(fp, "%s: %d<br>\n", args->game_state->player[p->id].nick,
                args->game_state->player[p->id].coins);

        p = get_next_player(players_array, p->id);
      } while (p && p != *args->starting_turn);
      fclose(fp);
    }
  }

  if (choice && choice->func) {
    // Using function pointers...
    choice->func(args, players_array, deck, choice);
  }
}
