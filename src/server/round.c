/*
 round.c
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

/* The per-hand betting engine, the draw phase, and the showdown.  Split out of
 * server.c; the cross-file helpers it shares with server.c and variants.c are
 * declared in server_internal.h. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dc_time.h"
#include "game.h"
#include "net.h"
#include "server.h"
#include "server_internal.h"
#include "util.h"

#define MAX_DISCARDS 4

/* Deliberately high: normal deliberation (and bots' 2-10s delays) routinely
 * exceed a few seconds, so only flag waits long enough to suggest a real stall
 * rather than thinking. (action_timeout defaults to 30s.) */
#define RECV_WAIT_WARN_MS 15000 /* waited this long for a player's input */

/* A client's draw request: which hole cards it wants to discard. */
typedef struct {
  uint8_t discard_count;
  uint8_t discard_indices[MAX_DISCARDS];
} DrawRequestMsg_t;

static void server_handle_call(GameState_t *game_state, uint32_t *total_paid, const uint8_t turn_id,
                               uint32_t *total_bets_plus_raises) {
  uint32_t owed = *total_bets_plus_raises - *total_paid;
  game_state->player[turn_id].coins -= owed;
  *total_paid += owed;
  game_state->pot += owed;
}

void server_handle_ante(GameState_t *game_state, const uint32_t amount) {
  Player_t *dealer = &game_state->player[game_state->dealer_id];
  Player_t *turn = dealer;
  do {
    if (game_state->player[turn->id].in) {
      game_state->player[turn->id].coins -= amount;
      game_state->pot += amount;
    }
    turn = get_next_player(game_state->player, turn->id);
  } while (turn && turn != dealer);
}

static void server_handle_bet(GameState_t *game_state, uint32_t *total_paid, const uint8_t turn_id,
                              const uint32_t amount, uint32_t *total_bets_plus_raises) {
  game_state->player[turn_id].coins -= amount;
  *total_paid += amount;
  *total_bets_plus_raises += amount;
  game_state->prev_bet_amount = amount;
  game_state->pot += amount;
}

// On Ubuntu 24.04 arm64: error: conflicting types for ‘raise’; so I've given
// this a more unique name now
static void server_handle_raise(GameState_t *game_state, uint32_t *total_paid,
                                const uint8_t turn_id, const uint32_t amount,
                                uint32_t *total_bets_plus_raises) {
  server_handle_call(game_state, total_paid, turn_id, total_bets_plus_raises);
  server_handle_bet(game_state, total_paid, turn_id, amount, total_bets_plus_raises);
  game_state->raises_remaining--;
}

void handle_sort_hand(POKEVAL_Hand_9 *real_hand, const bool is_lowball, const bool deuces_wild) {
  /* When deuces are wild, select the best 5-card hand using the wild-aware
   * evaluator so that a 2 is never dropped in favour of a higher non-wild
   * kicker when picking from a 6- or 7-card stud hand. */
  POKEVAL_Hand_5 tmp_hand = deuces_wild ? POKEVAL_hand5_from_hand7_wild(real_hand, DH_CARD_TWO)
                                        : POKEVAL_hand5_from_hand7(real_hand);
  if (!is_lowball)
    POKEVAL_sort_hand(&tmp_hand);
  else
    POKEVAL_sort_hand_lowball(&tmp_hand);
  /* POKEVAL_sort_hand mutates aces from DH_CARD_ACE (1) to POKEVAL_ACE (14)
   * so its own straight/broadway detector sees them at the top of the sort.
   * That mutation is fine for pokeval's internal use but must not bleed into
   * the broadcast hand — clients (and our own JSON hand log) expect cards
   * with face_val 1..13.  Restore aces before we copy the sorted hand into
   * the broadcast buffer. */
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
    if (tmp_hand.card[i].face_val == POKEVAL_ACE)
      tmp_hand.card[i].face_val = DH_CARD_ACE;
  }
  memcpy(&real_hand->card[0], &tmp_hand.card[0], sizeof(tmp_hand.card));

  /* Ensure unused card slots are NULL */
  real_hand->card[5] = DH_card_null;
  real_hand->card[6] = DH_card_null;
  real_hand->card[7] = DH_card_null;
  real_hand->card[8] = DH_card_null;
}

ELoop_t handle_draw(ArgsBroadcastGameState_t *args, tcpme_socket_t sock, const int8_t id,
                    DH_Deck *deck) {
  verbose_puts("sending draw prompt");
  if (send_opcode(sock, MSG_DRAW_PROMPT) != 0) {
    fputs("Failed to send draw prompt\n", stderr);
    return LOOP_ERROR;
  }

  DrawRequestMsg_t req;
  uint8_t buffer[32] = {0};
  bool timed_out = false;

  uint32_t wait_ms = args->game_settings->action_timeout_ms;
  uint32_t start = dc_get_ticks();
  for (;;) {
    uint32_t msg_size = 0;

    while (dc_get_ticks() - start < wait_ms) {
      register_new_client(args);
      int num_ready = tcpme_check_sockets(args->socket_set, 10);
      if (num_ready == -1) {
        fprintf(stderr, "tcpme_check_sockets: %s\n", tcpme_get_error());
        return LOOP_ERROR;
      }
      if (num_ready > 0) {
        if (tcpme_socket_ready(args->socket_set, sock)) {
          uint32_t size_net = 0;
          if (recv_all_tcp(sock, &size_net, sizeof(size_net)) > 0) {
            msg_size = tcpme_get_be32((const uint8_t *)&size_net);
            break;
          } else {
            remove_disconnected_player(args, id);
            broadcast_game_state(args);
            return LOOP_BREAK;
          }
        } else {
          if (handle_disconnections(args))
            broadcast_game_state(args);
          if (args->game_state->player_count == 1)
            return LOOP_BREAK;
        }
      }
    }

    if (msg_size == 0) {
      /* timed out — treat as stand pat, track consecutive timeouts */
      args->player_timeouts[id]++;
      const uint8_t m = args->config->action_timeout_max;
      if (m != 0 && !args->cli_args->disable_timeout && args->player_timeouts[id] == m) {
        remove_disconnected_player(args, id);
        printf("exceeded timeout threshold (%d): disconnecting %s\n", m,
               args->game_state->player[id].nick);
      }
      timed_out = true;
      break;
    }

    /* Payload buffer must fit both PING_RESPONSE (up to ~12 B) and
     * MSG_DRAW_REQUEST (exactly 7 B: 2 opcode + 1 count + 4 indices). */
    if (msg_size < 2 || msg_size > sizeof(buffer)) {
      fprintf(stderr, "[handle_draw] Invalid message size: %u\n", msg_size);
      return LOOP_ERROR;
    }
    memset(buffer, 0, sizeof(buffer));
    if (recv_all_tcp(sock, buffer, msg_size) <= 0) {
      remove_disconnected_player(args, id);
      broadcast_game_state(args);
      return LOOP_BREAK;
    }

    uint16_t opcode_be;
    memcpy(&opcode_be, buffer, sizeof(opcode_be));
    uint16_t opcode = tcpme_get_be16((const uint8_t *)&opcode_be);

    if (opcode == MSG_PING_RESPONSE)
      continue;

    if (opcode != MSG_DRAW_REQUEST) {
      fprintf(stderr, "[handle_draw] Unrecognised opcode 0x%04X\n", opcode);
      return LOOP_ERROR;
    }

    args->player_timeouts[id] = 0;
    break;
  }

  if (!timed_out) {
    uint32_t waited = dc_get_ticks() - start;
    if (waited >= RECV_WAIT_WARN_MS)
      dc_log(DC_LOG_WARN, "waited %ums for player %d's draw (slow input or stalled link)", waited,
             id);
  }

  uint8_t count = buffer[2];
  if (count > MAX_DISCARDS)
    return LOOP_ERROR;

  req.discard_count = count;
  memcpy(req.discard_indices, &buffer[3], MAX_DISCARDS); // copy all 4

  // printf("Player wants to discard %u cards: ", req.discard_count);
  for (int i = 0; i < req.discard_count; ++i) {
    // printf("%u ", req.discard_indices[i]);
    DH_discard_card(deck, args->real_hand[id].card[req.discard_indices[i]]);
    args->real_hand[id].card[req.discard_indices[i]] = DH_deal_top_card(deck);
    // puts("");
  }

  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof status_str, "%s drew %d", args->game_state->player[id].nick,
           req.discard_count);
  broadcast_status_message(args, status_str);

  if (req.discard_count > 0) {
    handle_sort_hand(&args->real_hand[id],
                     args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type,
                     args->deuces_wild);
    send_new_hand(sock, &args->real_hand[id], MAX_HAND_SIZE);
  }

  return timed_out ? LOOP_CONTINUE : LOOP_OK;
}

static EPlayerAction_t handle_check(PlayerActionMsg_t *action) {
  action->str = _("checked");
  return ACTION_CHECK;
}

static EPlayerAction_t handle_fold(GameState_t *game_state, POKEVAL_Hand_9 *real_hand,
                                   Player_t *turn, Player_t **starting_turn,
                                   PlayerActionMsg_t *action) {
  turn->in = false;
  for (int i = 0; i < MAX_HAND_SIZE; i++) {
    real_hand[turn->id].card[i] = DH_card_null;
    turn->hand.card[i] = DH_card_null;
  }
  game_state->player_count--;
  if (game_state->player_count > 1 && turn == *starting_turn)
    *starting_turn = get_next_player(game_state->player, turn->id);
  action->str = _("folded");
  return ACTION_FOLD;
}

static bool has_paid_all_bets(const uint32_t total_paid, const uint32_t total_bets_plus_raises) {
  return total_paid == total_bets_plus_raises;
}

void determine_winner(ArgsBroadcastGameState_t *args, RoundResults *results) {
  if (results->n_winners > 0)
    return;

  uint8_t pl_count = args->game_state->player_count;

  /* All players left the hand by showdown (e.g. disconnects): nothing to
   * compare or award.  Guards POKEVAL_compare_hands' assert(count > 0) and the
   * pot / n_winners divide below (a /0 in release where the assert is gone). */
  if (pl_count == 0)
    return;

  Player_t *ptr = *args->starting_turn;

  /* Do not truncate or sort hands here — clients display all dealt cards and
   * identify the best 5 themselves.  POKEVAL_compare_hands* already handles
   * 6- and 7-card hands via hand5_from_hand7 internally. */

  // When set to true, the opponents` cards will be revealed to all the players the next
  // time broadcast_game_state is called
  args->game_state->winner_declared = true;

  POKEVAL_NeedComparing *need_comparing = calloc_wrap(pl_count * sizeof(*need_comparing), 1);
  ptr = *args->starting_turn;
  for (uint8_t i = 0; i < pl_count; i++) {
    need_comparing[i].won = false;
    need_comparing[i].id = ptr->id;
    memcpy(&need_comparing[i].hand, &args->real_hand[ptr->id], sizeof(POKEVAL_Hand_9));
    ptr = get_next_player(args->game_state->player, ptr->id);
  }

  bool lowball = args->game_type == game_choices[CALIFORNIA_LOWBALL].game_type;
  if (args->game_type == game_choices[OMAHA].game_type)
    results->n_winners = POKEVAL_compare_hands_omaha(need_comparing, pl_count);
  else if (args->deuces_wild)
    results->n_winners = POKEVAL_compare_hands_wild(need_comparing, pl_count, DH_CARD_TWO);
  else
    results->n_winners = POKEVAL_compare_hands(need_comparing, pl_count, lowball);
  uint8_t winners = 0;

  uint32_t pot = args->game_state->pot;
  uint32_t share = pot / results->n_winners;
  uint32_t leftover = pot % results->n_winners;
  args->game_state->pot = leftover; // Remainder stays in the pot

  for (int i = 0; i < pl_count; i++) {
    if (!need_comparing[i].won)
      continue;

    results->id[winners++] = need_comparing[i].id;
    Player_t *winner = &args->game_state->player[need_comparing[i].id];
    winner->winner = true;

    short hand_rank = args->deuces_wild
                          ? POKEVAL_evaluate_hand_wild(need_comparing[i].hand_5, DH_CARD_TWO)
                          : POKEVAL_evaluate_hand(need_comparing[i].hand_5);
    char status_str[LEN_STATUS_STR];
    snprintf(status_str, sizeof status_str, "%s wins %" PRIu32 " with %s", winner->nick, share,
             POKEVAL_rank[hand_rank]);

    broadcast_status_message(args, status_str);

    if (args->cli_args->server_log_game_results_file) {
      FILE *fp = fopen(args->cli_args->server_log_game_results_file, "a");
      if (!fp)
        perror("fopen");
      else {
        fprintf(fp, "pot: %u<br>\n", pot);
        fprintf(fp, "%s\n\n", status_str);
        fclose(fp);
      }
    }
    winner->coins += share;
  }
  log_hands_json(args, need_comparing, pl_count, pot, false);
  free(need_comparing);
  broadcast_game_state(args);
}

void award_last_player_in_game(ArgsBroadcastGameState_t *args, Player_t *turn,
                               RoundResults *results) {
  if (!turn->is_connected || !turn->in) {
    // fprintf(stderr, "turn->id: %d | %d\n", turn->id, __LINE__);
    turn = get_next_player(args->game_state->player, 0);
    if (!turn)
      return;
  }
  turn->winner = true;
  char status_str[LEN_STATUS_STR] = {0};
  snprintf(status_str, sizeof(status_str), "%s wins %d", turn->nick, args->game_state->pot);
  broadcast_status_message(args, status_str);
  if (args->cli_args->server_log_game_results_file) {
    FILE *fp = fopen(args->cli_args->server_log_game_results_file, "a");
    if (!fp)
      perror("fopen");
    else {
      fprintf(fp, "pot: %d<br>\n", args->game_state->pot);
      fprintf(fp, "%s\n\n", status_str);
      fclose(fp);
    }
  }

  args->game_state->winner_declared = true;
  results->n_winners = 1;
  // fprintf(stderr, "winner id from fold: %d\n", turn->id);
  results->id[0] = turn->id;
  log_hands_fold_json(args, turn, args->game_state->pot);
  turn->coins += args->game_state->pot;
  args->game_state->pot = 0;
  return;
}

RoundResults handle_round_real(ArgsBroadcastGameState_t *args, uint32_t initial_bet,
                               int8_t initial_paid_id) {
  args->game_state->raises_remaining = args->config->max_raises;
  args->game_state->prev_bet_amount = initial_bet;

  Player_t *turn;

  RoundResults results = {0};

  turn = *args->starting_turn;
  args->game_state->round_opener_id = (int8_t)turn->id;
  uint32_t total_bets_plus_raises = initial_bet;
  uint32_t player_total_paid[MAX_PLAYERS] = {0};
  if (initial_paid_id >= 0 && initial_paid_id < MAX_PLAYERS)
    player_total_paid[initial_paid_id] = initial_bet;
  uint8_t num_turns = 0;

  do {
    args->turn_id = turn->id;
    broadcast_turn_id(args);
    broadcast_game_state(args);

    uint32_t wait_ms = args->game_settings->action_timeout_ms;
    uint32_t start = dc_get_ticks();
    PlayerActionMsg_t action = {0};

    uint32_t owed = (player_total_paid[turn->id] < total_bets_plus_raises)
                        ? total_bets_plus_raises - player_total_paid[turn->id]
                        : 0;
    // In the bring-in round (initial_bet > 0) and before anyone has completed or
    // raised (total == initial_bet), use the more accurate Complete opcodes.
    bool bringin_round_unopened = (initial_bet > 0 && total_bets_plus_raises == initial_bet);
    uint16_t opcode;
    if (owed > 0)
      opcode = bringin_round_unopened ? MSG_CALL_COMPLETE_FOLD : MSG_CALL_RAISE_FOLD;
    else
      opcode = bringin_round_unopened ? MSG_COMPLETE_CHECK_FOLD : MSG_BET_CHECK_FOLD;
    if (send_opcode(args->clients[turn->id], opcode) != 0)
      fputs("Error sending action prompt", stderr);
    dc_log(DC_LOG_DEBUG, "sent action-prompt opcode 0x%04X to player %d", opcode, turn->id);

    while (dc_get_ticks() - start < wait_ms) {
      register_new_client(args);
      // fprintf(stderr, "Waiting for action from %d\n", args->turn_id);
      int n_ready = tcpme_check_sockets(args->socket_set, 100); // wait up to 100ms
      if (n_ready > 0) {
        // If this socket is ready (the player who's turn it is), they either
        // disconnected, or have sent an action.
        if (tcpme_socket_ready(args->socket_set, args->clients[turn->id])) {
          uint16_t kb_opcode = 0;
          int8_t kb_target = -1;
          ETurnMsg_t msg_type =
              recv_turn_player_msg(args->clients[turn->id], &action, &kb_opcode, &kb_target);
          if (msg_type == TURN_MSG_KICK_BAN) {
            /* Admin is on-turn and sent a kick/ban instead of a game action.
             * Process it and keep waiting for their game action. */
            if (kb_target >= 0 && kb_target != turn->id) {
              if (kb_opcode == MSG_KICK_PLAYER)
                kick_player(args, kb_target);
              else
                ban_player(args, kb_target);
              broadcast_game_state(args);
            }
            continue;
          } else if (msg_type == TURN_MSG_ACTION) {
            if (opcode == MSG_BET_CHECK_FOLD || opcode == MSG_COMPLETE_CHECK_FOLD) {
              switch (action.action) {
              case ACTION_CHECK:
                handle_check(&action);
                break;
              case ACTION_BET:
                server_handle_bet(args->game_state, &player_total_paid[turn->id], turn->id,
                                  action.amount, &total_bets_plus_raises);
                action.str = (opcode == MSG_COMPLETE_CHECK_FOLD) ? _("completed ") : _("bet ");
                break;
              case ACTION_FOLD:
                handle_fold(args->game_state, args->real_hand, turn, args->starting_turn, &action);
                break;
              default:
                fprintf(stderr, "Invalid Action %u received for opcode 0x%04X from player %d\n",
                        action.action, opcode, turn->id);
                remove_disconnected_player(args, turn->id);
              }
            } else {
              switch (action.action) {
              case ACTION_CALL:
                server_handle_call(args->game_state, &player_total_paid[turn->id], turn->id,
                                   &total_bets_plus_raises);
                action.str = _("called");
                break;
              case ACTION_BET:
                // Complete: raise to full bet from the bring-in (MSG_CALL_COMPLETE_FOLD).
                if (opcode == MSG_CALL_COMPLETE_FOLD) {
                  server_handle_bet(args->game_state, &player_total_paid[turn->id], turn->id,
                                    action.amount, &total_bets_plus_raises);
                  action.str = _("completed ");
                } else {
                  fputs("BET received unexpectedly; ignoring\n", stderr);
                }
                break;
              case ACTION_RAISE:
                if (args->game_state->raises_remaining > 0) {
                  if (action.amount < args->game_state->prev_bet_amount) {
                    fprintf(stderr,
                            "Raise amount %" PRIu32 " below minimum %" PRIu32
                            " from player %d; treating as call\n",
                            action.amount, args->game_state->prev_bet_amount, turn->id);
                    server_handle_call(args->game_state, &player_total_paid[turn->id], turn->id,
                                       &total_bets_plus_raises);
                    action.str = _("called");
                  } else {
                    server_handle_raise(args->game_state, &player_total_paid[turn->id], turn->id,
                                        action.amount, &total_bets_plus_raises);
                    action.str = _("raised ");
                  }
                } else
                  fputs("Raise received; however, max raises has been reached. The client should "
                        "not be able to send a raise\n",
                        stderr);
                break;
              case ACTION_FOLD:
                handle_fold(args->game_state, args->real_hand, turn, args->starting_turn, &action);
                break;
              default:
                fputs("Invalid Action received\nThe client is writing checks their body "
                      "can't cash.\n",
                      stderr);
                remove_disconnected_player(args, turn->id);
              }
            }
          } else { /* TURN_MSG_DISCONNECT */
            remove_disconnected_player(args, args->turn_id);
            break;
          }
          break;
        } else {
          // There is data to be read, but not from the player whos turn it is, so probably
          // a disconnect by another client.
          handle_disconnections(args);
          if (args->game_state->player_count == 1)
            break;
          continue;
        }
      }
    }

    if (action.action != 0) {
      uint32_t waited = dc_get_ticks() - start;
      if (waited >= RECV_WAIT_WARN_MS)
        dc_log(DC_LOG_WARN, "waited %ums for player %d's action (slow input or stalled link)",
               waited, turn->id);
    }

    char status_str[LEN_STATUS_STR] = {0};
    if (args->game_state->player_count > 1) {
      if (turn->is_connected) {
        if (action.action == 0) {
          args->player_timeouts[turn->id]++;
          if (!has_paid_all_bets(player_total_paid[turn->id], total_bets_plus_raises)) {
            action.action =
                handle_fold(args->game_state, args->real_hand, turn, args->starting_turn, &action);
          } else if (owed == 0) {
            action.action = handle_check(&action);
          }
          const uint8_t m = args->config->action_timeout_max;
          if (m != 0 && !args->cli_args->disable_timeout && args->player_timeouts[turn->id] == m) {
            remove_disconnected_player(args, args->turn_id);
            printf("exceeded timeout threshold (%d): disconnecting %s\n", m, turn->nick);
          }
        } else
          args->player_timeouts[turn->id] = 0;

        if (action.amount > 0)
          snprintf(status_str, sizeof status_str, "%s %s%d", turn->nick, action.str, action.amount);
        else
          snprintf(status_str, sizeof status_str, "%s %s", turn->nick, action.str);
      }

      broadcast_status_message(args, status_str);
      verbose_puts(status_str);

      // player_count might be 1 now, if a player folded due to an action timeout
      if (args->game_state->player_count == 1) {
        // broadcast_game_state(args);
        award_last_player_in_game(args, turn, &results);
        break;
      }

      turn = get_next_player(args->game_state->player, turn->id);
      num_turns++;
      if (num_turns >= args->game_state->player_count) {
        if (total_bets_plus_raises == 0) {
          if (turn == *args->starting_turn)
            break;
        } else if (has_paid_all_bets(player_total_paid[turn->id], total_bets_plus_raises)) {
          break; // Everyone either checked or paid all bets and raises
        }
      }

      if (results.n_winners > 0) {
        break;
      }
    } else {
      if (action.str != NULL) {
        snprintf(status_str, sizeof status_str, "%s %s", turn->nick, action.str);
        broadcast_status_message(args, status_str);
      }
      award_last_player_in_game(args, turn, &results);
      break;
    }
  } while (true);

  return results;
}
