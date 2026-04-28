/*
 bot.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include <pokeval.h>

#include "client.h"
#include "game.h"
#include "getlongopt.h"
#include "globals.h"
#include "net.h"
#include "types.h"
#include "util.h"

/*
 * Choose which cards to discard for draw games.
 *
 * Returns the number of discards and fills discard_indices[] with the
 * 0-based indices of cards to throw away (relative to hand->card[]).
 *
 * Strategy (standard high-hand games):
 *   - Straight flush / flush / straight / full house / quads: stand pat
 *   - Trips: keep 3, draw 2
 *   - Two pair: keep 4, draw 1
 *   - One pair: keep pair, draw 3
 *   - 4-to-a-flush: keep 4 suited cards, draw 1
 *   - Otherwise: keep the single best card, draw 4
 *
 * Lowball (California lowball): keep unpaired cards with face <= 8
 * (ace counts low), discard everything else.
 */
static uint8_t bot_choose_discards(const POKEVAL_Hand_9 *hand, uint8_t hand_size, bool lowball,
                                   uint8_t *discard_indices) {
  /* Collect valid card indices */
  int valid[POKEVAL_HAND_SIZE];
  int n_valid = 0;
  for (int i = 0; i < hand_size && n_valid < POKEVAL_HAND_SIZE; i++) {
    if (!DH_is_card_null(hand->card[i]))
      valid[n_valid++] = i;
  }
  if (n_valid == 0)
    return 0;

  /* Face counts [0..14]: index = face_val, ace-low = 1, ace-high = 14 */
  int face_count[15] = {0};
  /* Suit counts [0..3] */
  int suit_count[4] = {0};

  for (int k = 0; k < n_valid; k++) {
    DH_Card c = hand->card[valid[k]];
    face_count[c.face_val]++;
    suit_count[c.suit]++;
  }

  /* ---- Lowball strategy ---- */
  if (lowball) {
    /* Keep unpaired low cards (ace counts as 1, so face_val==1 is low).
     * Discard any card that is paired or has face > 8. */
    uint8_t n_discard = 0;
    for (int k = 0; k < n_valid; k++) {
      DH_Card c = hand->card[valid[k]];
      bool high = (c.face_val > 8); /* 9, T, J, Q, K are bad */
      bool paired = (face_count[c.face_val] > 1);
      if (high || paired)
        discard_indices[n_discard++] = (uint8_t)valid[k];
    }
    return n_discard;
  }

  /* ---- Standard high-hand strategy ---- */

  /* Find trips, pairs */
  int trip_face = -1;
  int pairs[2] = {-1, -1};
  int n_pairs = 0;

  for (int f = 1; f <= 14; f++) {
    if (face_count[f] == 3)
      trip_face = f;
    else if (face_count[f] == 2 && n_pairs < 2)
      pairs[n_pairs++] = f;
  }

  /* Check for 4 (or 5) cards of the same suit */
  int flush_suit = -1;
  for (int s = 0; s < 4; s++) {
    if (suit_count[s] >= 4) {
      flush_suit = s;
      break;
    }
  }

  /* Evaluate the full 5-card hand to detect straights/flushes */
  POKEVAL_Hand_5 h5 = {0};
  for (int k = 0; k < n_valid && k < POKEVAL_HAND_SIZE; k++)
    h5.card[k] = hand->card[valid[k]];
  short rank = POKEVAL_evaluate_hand(h5);

  /* Stand pat on any made hand of straight or better */
  if (rank >= POKEVAL_STRAIGHT)
    return 0;

  /* Trips: discard the two non-matching cards */
  if (trip_face >= 0) {
    uint8_t n_discard = 0;
    for (int k = 0; k < n_valid; k++) {
      if (hand->card[valid[k]].face_val != trip_face)
        discard_indices[n_discard++] = (uint8_t)valid[k];
    }
    return n_discard;
  }

  /* Two pair: discard the single kicker */
  if (n_pairs == 2) {
    uint8_t n_discard = 0;
    for (int k = 0; k < n_valid; k++) {
      int fv = hand->card[valid[k]].face_val;
      if (fv != pairs[0] && fv != pairs[1])
        discard_indices[n_discard++] = (uint8_t)valid[k];
    }
    return n_discard;
  }

  /* One pair: 50% keep pair only (draw 3), 50% keep pair + best kicker (draw 2) */
  if (n_pairs == 1) {
    bool keep_kicker = (pcg32_boundedrand_r(&rng, 2) == 0);
    int kicker_idx = -1;
    if (keep_kicker) {
      int kicker_val = -1;
      for (int k = 0; k < n_valid; k++) {
        int fv = hand->card[valid[k]].face_val;
        if (fv == pairs[0])
          continue;
        if (fv == DH_CARD_ACE)
          fv = POKEVAL_ACE;
        if (fv > kicker_val) {
          kicker_val = fv;
          kicker_idx = valid[k];
        }
      }
    }
    uint8_t n_discard = 0;
    for (int k = 0; k < n_valid; k++) {
      if (hand->card[valid[k]].face_val != pairs[0] && valid[k] != kicker_idx)
        discard_indices[n_discard++] = (uint8_t)valid[k];
    }
    return n_discard;
  }

  /* 4-to-a-flush: discard the one off-suit card */
  if (flush_suit >= 0) {
    uint8_t n_discard = 0;
    for (int k = 0; k < n_valid; k++) {
      if (hand->card[valid[k]].suit != flush_suit)
        discard_indices[n_discard++] = (uint8_t)valid[k];
    }
    return n_discard;
  }

  /* High card: find best card (ace counts high) */
  int best_idx = valid[0];
  int best_val = hand->card[valid[0]].face_val;
  if (best_val == DH_CARD_ACE)
    best_val = POKEVAL_ACE;
  for (int k = 1; k < n_valid; k++) {
    int fv = hand->card[valid[k]].face_val;
    if (fv == DH_CARD_ACE)
      fv = POKEVAL_ACE;
    if (fv > best_val) {
      best_val = fv;
      best_idx = valid[k];
    }
  }

  /* If holding an ace: 80% draw 3 (keep ace + 2nd best), 20% draw 4 (keep ace only) */
  int second_idx = -1;
  if (best_val == POKEVAL_ACE && pcg32_boundedrand_r(&rng, 100) >= 20) {
    int second_val = -1;
    for (int k = 0; k < n_valid; k++) {
      if (valid[k] == best_idx)
        continue;
      int fv = hand->card[valid[k]].face_val;
      if (fv == DH_CARD_ACE)
        fv = POKEVAL_ACE;
      if (fv > second_val) {
        second_val = fv;
        second_idx = valid[k];
      }
    }
  }

  uint8_t n_discard = 0;
  for (int k = 0; k < n_valid; k++) {
    if (valid[k] != best_idx && valid[k] != second_idx)
      discard_indices[n_discard++] = (uint8_t)valid[k];
  }
  return n_discard;
}

#define BOT_DEFAULT_NICK "Bot"
#define BOT_DEFAULT_HOST "127.0.0.1"
#define BOT_DEFAULT_PORT 22777
#define BOT_POLL_MS 10

static void print_usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [options]\n"
          "  --host <host>   Server host (default: " BOT_DEFAULT_HOST ")\n"
          "  --port <port>   Server port (default: %d)\n"
          "  --nick <nick>   Bot nickname (default: " BOT_DEFAULT_NICK ")\n"
          "  --verbose       Enable verbose debug output\n"
          "\n"
          "Set DC_PASSWORD in the environment to authenticate with a\n"
          "password-protected server.\n",
          argv0, BOT_DEFAULT_PORT);
}

int main(int argc, char *argv[]) {
  const char *host = BOT_DEFAULT_HOST;
  uint16_t port = BOT_DEFAULT_PORT;
  const char *nick = BOT_DEFAULT_NICK;

  enum { OPT_HOST = 1, OPT_PORT, OPT_NICK, OPT_HELP, OPT_VERBOSE };
  static const glopt_option_t options[] = {
      {"host", GLOPT_REQUIRED_ARG, OPT_HOST, 0}, {"port", GLOPT_REQUIRED_ARG, OPT_PORT, 0},
      {"nick", GLOPT_REQUIRED_ARG, OPT_NICK, 0}, {"help", GLOPT_NO_ARG, OPT_HELP, 'h'},
      {"verbose", GLOPT_NO_ARG, OPT_VERBOSE, 0}, {NULL, 0, 0, 0},
  };
  glopt_parser_t parser;
  glopt_init(&parser, options);
  int opt;
  while ((opt = glopt_next(&parser, argc, argv)) != -1) {
    switch (opt) {
    case OPT_HOST:
      host = parser.optarg;
      break;
    case OPT_PORT:
      port = (uint16_t)atoi(parser.optarg);
      break;
    case OPT_NICK:
      nick = parser.optarg;
      break;
    case OPT_HELP:
      print_usage(argv[0]);
      return 0;
    case OPT_VERBOSE:
      verbose = true;
      break;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  pcg_srand_auto();

  if (SDL_Init(0) == -1) {
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }
  if (tcpme_init() != 0) {
    fprintf(stderr, "tcpme_init failed: %s\n", tcpme_get_error());
    SDL_Quit();
    return 1;
  }

  SocketContext_t socket_ctx = {0};
  const char *password = getenv("DC_PASSWORD");
  if (!bot_connect(host, port, nick, password ? password : "", &socket_ctx)) {
    fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
    tcpme_quit();
    SDL_Quit();
    return 1;
  }

  printf("Connected to %s:%d as \"%s\"\n", host, port, nick);

  GameSettings_t game_settings = {0};
  {
    ERecvStatus_t s;
    do {
      s = recv_game_settings(socket_ctx.sock, socket_ctx.set, &game_settings);
      if (s == RECV_ERROR)
        break;
    } while (s == RECV_NOTHING);
    if (s != RECV_SUCCESS) {
      fprintf(stderr, "Failed to receive game settings\n");
      socket_cleanup(&socket_ctx);
      tcpme_quit();
      SDL_Quit();
      return 1;
    }
  }

  int8_t my_id = game_settings.client_id;
  printf("Assigned player ID: %d\n", my_id);

  GameState_t game_state = {0};
  ClientState_t client_state = {0};
  bool game_select_sent = false;
  Uint32 game_select_after = 0; /* SDL tick time after which we may deal */
  Uint32 action_after = 0;      /* SDL tick time after which we may act */
  bool was_aggressor = false;   /* true if we bet/raised this hand (for c-bet logic) */
  bool checked_strong = false;  /* true if we checked a strong hand to set up a check-raise */

  while (true) {
    SDL_Delay(BOT_POLL_MS);

    ERecvStatus_t status = recv_game_state(&socket_ctx, &game_state, &client_state, my_id);
    if (status == RECV_ERROR) {
      fputs("Connection lost\n", stderr);
      break;
    }

    /* ---- Action delay timer (3–15 s) ---- */
    bool any_action = (client_state.bet_check_fold || client_state.call_raise_fold ||
                       client_state.call_complete_fold || client_state.complete_check_fold ||
                       client_state.do_discard_draw);
    if (!any_action)
      action_after = 0;
    else if (action_after == 0) {
      Uint32 delay_ms = 2000 + pcg32_boundedrand_r(&rng, 4001); /* 2000–6000 ms */
      action_after = SDL_GetTicks() + delay_ms;
    }

    /* Nothing received and no action timer has fired: nothing to do */
    if (status == RECV_NOTHING) {
      if (!any_action || SDL_GetTicks() < action_after)
        continue;
    }

    /* Reset the game-select guard each time the server returns to the menu */
    if (game_state.at_menu) {
      if (!game_select_sent) {
        /* Schedule a random 4–10 s delay before dealing, but only once per
           menu session and only when there is at least one other player. */
        int n_connected = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (game_state.player[i].is_connected)
            n_connected++;
        }
        if (game_select_after == 0 && n_connected >= 2) {
          Uint32 delay_ms = 4000 + pcg32_boundedrand_r(&rng, 6001); /* 4000–10000 ms */
          game_select_after = SDL_GetTicks() + delay_ms;
        }
      } else {
        game_select_sent = false;
        game_select_after = 0;
        was_aggressor = false; /* new hand */
        checked_strong = false;
      }
    }

    /* Game selection: send once we are the dealer, there are >= 2 players,
       and the random delay has elapsed. */
    if (game_state.at_menu && game_state.dealer_id == my_id && !game_select_sent &&
        game_select_after != 0 && SDL_GetTicks() >= game_select_after) {
      int pick = (int)pcg32_boundedrand_r(&rng, MAX_CHOICES);
      const GameChoice_t *choice = &game_choices[pick];
      bool deuces_wild = choice->deuces_wild_compatible && (pcg32_boundedrand_r(&rng, 2) == 0);
      if (send_game_select(socket_ctx.sock, choice->game_type, deuces_wild) == 0) {
        printf("Selected game: %s%s\n", choice->str, deuces_wild ? " (deuces wild)" : "");
        game_select_sent = true;
      }
      continue;
    }

    /* ---- Betting / discard actions — only after delay has elapsed ---- */
    if (!any_action || SDL_GetTicks() < action_after)
      continue;
    action_after = 0;

    /* Evaluate the current hand.  Use the wild-card evaluator when deuces are
     * wild so hand strength is correctly assessed in those games. */
    POKEVAL_Hand_5 h5 = {0};
    {
      int k5 = 0;
      for (int i = 0; i < MAX_HAND_SIZE && k5 < POKEVAL_HAND_SIZE; i++) {
        DH_Card c = game_state.player[my_id].hand.card[i];
        if (!DH_is_card_null(c) && !DH_is_card_back(c))
          h5.card[k5++] = c;
      }
    }
    hand_rank_t hand_rank = client_state.deuces_wild
                                ? (hand_rank_t)POKEVAL_evaluate_hand_wild(h5, DH_CARD_TWO)
                                : (hand_rank_t)POKEVAL_evaluate_hand(h5);

    /* Graduated hand strength:
     *   0 = high card
     *   1 = one pair
     *   2 = two pair
     *   3 = three of a kind / straight / flush
     *   4 = full house or better */
    int strength = 0;
    if (hand_rank >= POKEVAL_FULL_HOUSE)
      strength = 4;
    else if (hand_rank >= POKEVAL_THREE_OF_A_KIND)
      strength = 3;
    else if (hand_rank == POKEVAL_TWO_PAIR)
      strength = 2;
    else if (hand_rank == POKEVAL_PAIR)
      strength = 1;

    /* Detect drawing hands for semi-bluff purposes.
     * Only meaningful when we don't already have a made hand (strength < 3).
     *   draw_strength 0 = no draw
     *   draw_strength 1 = open-ended straight draw (4 consecutive ranks)
     *   draw_strength 2 = flush draw (4 cards of the same suit) */
    int draw_strength = 0;
    if (strength < 3) {
      int suit_cnt[4] = {0};
      int face_bits = 0; /* bit i = face value i present; ace sets both bit 1 and bit 14 */
      for (int i = 0; i < MAX_HAND_SIZE; i++) {
        DH_Card c = game_state.player[my_id].hand.card[i];
        if (DH_is_card_null(c) || DH_is_card_back(c))
          continue;
        if (c.suit >= 0 && c.suit < 4)
          suit_cnt[c.suit]++;
        if (c.face_val >= 1 && c.face_val <= 13)
          face_bits |= (1 << c.face_val);
        if (c.face_val == DH_CARD_ACE)
          face_bits |= (1 << 14); /* ace also plays high */
      }
      for (int s = 0; s < 4; s++) {
        if (suit_cnt[s] == 4) {
          draw_strength = 2;
          break;
        }
      }
      if (draw_strength == 0) {
        for (int f = 1; f <= 10; f++) {
          int mask = (1 << f) | (1 << (f + 1)) | (1 << (f + 2)) | (1 << (f + 3));
          if ((face_bits & mask) == mask) {
            draw_strength = 1;
            break;
          }
        }
      }
    }

    verbose_printf("hand: strength=%d draw=%d\n", strength, draw_strength);

    /* Count opponents who are still active in this hand. */
    int active_opponents = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (i != my_id && game_state.player[i].in && game_state.player[i].is_connected)
        active_opponents++;
    }

    /* Pot odds: what fraction of (pot + call) the call represents (pct).
     * A lower value means better pot odds and more reason to call. */
    uint32_t call_cost = game_state.prev_bet_amount;
    int pot_odds_pct = (call_cost > 0 && (game_state.pot + call_cost) > 0)
                           ? (int)(100u * call_cost / (game_state.pot + call_cost))
                           : 0;

    bool can_raise = (game_state.raises_remaining > 0);
    /* Pick the smallest configured amount that meets the minimum-raise requirement.
     * Falls back to the largest amount if none qualifies (shouldn't happen in practice). */
    uint32_t bet_amount = game_settings.bet_amounts[game_settings.bet_amount_count - 1];
    for (uint8_t i = 0; i < game_settings.bet_amount_count; i++) {
      if (game_settings.bet_amounts[i] >= game_state.prev_bet_amount) {
        bet_amount = game_settings.bet_amounts[i];
        break;
      }
    }

    /* Flags are set by the server sending the corresponding opcode directly to
       this client, so no need to check turn_id. */
    int rc = 0;
    if (client_state.bet_check_fold) {
      /* Base open-bet rates by strength (high-card=10%, pair=20%, two-pair=42%,
       * trips/str/flush=65%, FH+=85%).  Bonuses:
       *   +25% c-bet when we were the aggressor last round.
       *   flush draw: raise floor to 38% (semi-bluff).
       *   OESD:       raise floor to 22% (semi-bluff).
       *   multiway (3+ opponents): scale down by ~35% — bluffs work less often. */
      static const int base_open_pct[5] = {10, 20, 42, 65, 85};
      int bet_pct = base_open_pct[strength];
      if (was_aggressor)
        bet_pct += 25;
      if (draw_strength == 2 && bet_pct < 38)
        bet_pct = 38;
      else if (draw_strength == 1 && bet_pct < 22)
        bet_pct = 22;
      if (active_opponents >= 3)
        bet_pct = bet_pct * 65 / 100;
      if (bet_pct > 95)
        bet_pct = 95;

      if (can_raise && (int)pcg32_boundedrand_r(&rng, 100) < bet_pct) {
        verbose_printf("bet_pct=%d\n", bet_pct);
        rc = send_player_action(&client_state, socket_ctx.sock, ACTION_BET, bet_amount);
        was_aggressor = true;
        checked_strong = false;
      } else {
        verbose_puts("(open, no bet)");
        rc = send_player_action(&client_state, socket_ctx.sock, ACTION_CHECK, 0);
        /* Remember if we checked a strong hand — we may be setting up a check-raise. */
        checked_strong = (strength >= 3);
      }
      /* send_player_action clears bet_check_fold and call_raise_fold */
    } else if (client_state.call_raise_fold) {
      if (checked_strong && can_raise) {
        /* Check-raise: we checked a strong hand last round hoping someone would bet.
         * They did — almost always raise, occasionally just call to keep them guessing. */
        checked_strong = false;
        if (pcg32_boundedrand_r(&rng, 100) < 88) {
          verbose_puts("check-raise");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_RAISE, bet_amount);
          was_aggressor = true;
        } else {
          verbose_puts("(check-raise disguise)");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_CALL, 0);
        }
      } else if (strength >= 4) {
        checked_strong = false;
        /* Monster: raise most of the time, otherwise call */
        if (can_raise && pcg32_boundedrand_r(&rng, 100) < 75) {
          verbose_puts("(monster)");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_RAISE, bet_amount);
          was_aggressor = true;
        } else {
          verbose_puts("(monster slow-play)");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_CALL, 0);
        }
      } else if (strength == 3) {
        checked_strong = false;
        /* Trips / straight / flush: raise half the time */
        if (can_raise && pcg32_boundedrand_r(&rng, 100) < 50) {
          verbose_puts("(trips/str/flush)");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_RAISE, bet_amount);
          was_aggressor = true;
        } else {
          verbose_puts("(trips/str/flush)");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_CALL, 0);
        }
      } else if (strength == 2) {
        /* Two pair: call unless pot odds are poor; tighten with more opponents */
        int fold_pct = 10 + active_opponents * 8; /* ~18% hu, 26% vs 2, 34% vs 3+ */
        if (pot_odds_pct > 45 || (int)pcg32_boundedrand_r(&rng, 100) < fold_pct) {
          verbose_printf("(two pair, pot_odds=%d%% fold_pct=%d)\n", pot_odds_pct, fold_pct);
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_FOLD, 0);
          was_aggressor = false;
        } else {
          verbose_puts("(two pair)");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_CALL, 0);
        }
      } else if (strength == 1) {
        /* One pair: pot-odds driven.  Approximate equity shrinks with more opponents.
         * With a flush draw on top, semi-bluff raise occasionally. */
        if (draw_strength == 2 && can_raise && pcg32_boundedrand_r(&rng, 100) < 25) {
          verbose_puts("(pair+flush draw semi-bluff)");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_RAISE, bet_amount);
          was_aggressor = true;
        } else {
          int equity_pct = (active_opponents <= 1) ? 38 : (active_opponents == 2 ? 26 : 18);
          if (pot_odds_pct > equity_pct) {
            verbose_printf("(pair, pot_odds=%d%% > equity=%d%%)\n", pot_odds_pct, equity_pct);
            rc = send_player_action(&client_state, socket_ctx.sock, ACTION_FOLD, 0);
            was_aggressor = false;
          } else {
            verbose_puts("(pair)");
            rc = send_player_action(&client_state, socket_ctx.sock, ACTION_CALL, 0);
          }
        }
      } else {
        /* High card — bluff strategy:
         * Base:        raise 7%,  call (float) 20%, fold 73%.
         * C-bet line:  raise 22%, call 35%,         fold 43%.
         * Flush draw:  raise 30%+ (semi-bluff).
         * OESD:        raise 18%+ (semi-bluff).
         * Multiway:    halve raise pct, reduce float. */
        int raise_pct = was_aggressor ? 22 : 7;
        int call_pct = was_aggressor ? 35 : 20;
        if (draw_strength == 2 && raise_pct < 30)
          raise_pct = 30;
        else if (draw_strength == 1 && raise_pct < 18)
          raise_pct = 18;
        if (active_opponents >= 3) {
          raise_pct = raise_pct / 2;
          call_pct = call_pct * 2 / 3;
        }
        int r = (int)pcg32_boundedrand_r(&rng, 100);
        if (can_raise && r < raise_pct) {
          verbose_printf("(high card bluff, raise_pct=%d)\n", raise_pct);
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_RAISE, bet_amount);
          was_aggressor = true;
        } else if (r < raise_pct + call_pct) {
          verbose_puts("(high card float)");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_CALL, 0);
        } else {
          verbose_puts("(high card)");
          rc = send_player_action(&client_state, socket_ctx.sock, ACTION_FOLD, 0);
          was_aggressor = false;
        }
      }
    } else if (client_state.call_complete_fold) {
      /* Stud bring-in.  Mixed strategy: strong hands usually complete (raise) but
       * occasionally just call to disguise the hand; weak hands mostly fold but
       * sometimes bluff-complete or call.  Draw strength adds semi-bluff raises. */
      int raise_pct, call_pct;
      switch (strength) {
      case 4:
        raise_pct = 80;
        call_pct = 20;
        break; /* monster: slow-play 20% */
      case 3:
        raise_pct = 58;
        call_pct = 37;
        break; /* trips+: mix it up */
      case 2:
        raise_pct = 18;
        call_pct = 72;
        break; /* two pair: lean call */
      case 1:
        raise_pct = 10;
        call_pct = 68;
        break; /* pair: usually call */
      default:
        raise_pct = 7;
        call_pct = 20;
        break; /* high card: mostly fold */
      }
      if (draw_strength == 2)
        raise_pct += 15; /* flush draw: semi-bluff */
      else if (draw_strength == 1)
        raise_pct += 8;
      if (!can_raise) {
        call_pct += raise_pct;
        raise_pct = 0;
      }
      int r = (int)pcg32_boundedrand_r(&rng, 100);
      if (r < raise_pct) {
        verbose_printf("(bring-in, raise_pct=%d)\n", raise_pct);
        rc = send_player_action(&client_state, socket_ctx.sock, ACTION_BET, bet_amount);
        was_aggressor = true;
      } else if (r < raise_pct + call_pct) {
        verbose_puts("(bring-in)");
        rc = send_player_action(&client_state, socket_ctx.sock, ACTION_CALL, 0);
      } else {
        verbose_puts("(bring-in fold)");
        rc = send_player_action(&client_state, socket_ctx.sock, ACTION_FOLD, 0);
        was_aggressor = false;
      }
      client_state.call_complete_fold = false;
    } else if (client_state.complete_check_fold) {
      /* Bring-in player's free second action.  Never fold (it's free to check).
       * Strong hands usually complete (raise) but occasionally check to trap;
       * weaker hands mostly check with an occasional bluff-complete. */
      int raise_pct;
      switch (strength) {
      case 4:
        raise_pct = 82;
        break; /* monster: check-trap 18% */
      case 3:
        raise_pct = 55;
        break; /* trips: coin flip */
      case 2:
        raise_pct = 28;
        break; /* two pair: semi-aggressive */
      case 1:
        raise_pct = 14;
        break; /* pair: lean check */
      default:
        raise_pct = 8;
        break; /* high card: occasional bluff */
      }
      if (draw_strength == 2)
        raise_pct += 12;
      else if (draw_strength == 1)
        raise_pct += 6;
      if (!can_raise)
        raise_pct = 0;
      if ((int)pcg32_boundedrand_r(&rng, 100) < raise_pct) {
        verbose_printf("(raise_pct=%d)\n", raise_pct);
        rc = send_player_action(&client_state, socket_ctx.sock, ACTION_BET, bet_amount);
        was_aggressor = true;
      } else {
        verbose_puts("(free action)");
        rc = send_player_action(&client_state, socket_ctx.sock, ACTION_CHECK, 0);
      }
      client_state.complete_check_fold = false;
    } else if (client_state.do_discard_draw) {
      uint8_t discard_indices[MAX_HAND_SIZE] = {0};
      uint8_t hand_size =
          client_state.game_choice ? client_state.game_choice->hand_size : POKEVAL_HAND_SIZE;
      bool lowball =
          (client_state.game_choice && client_state.game_choice->g == CALIFORNIA_LOWBALL);
      uint8_t n_discards =
          bot_choose_discards(&game_state.player[my_id].hand, hand_size, lowball, discard_indices);
      verbose_printf("discarding %d card(s)\n", n_discards);
      rc = send_discards_request_new_cards(socket_ctx.sock, discard_indices, n_discards);
      client_state.do_discard_draw = false;
    }
    if (rc != 0) {
      fputs("Failed to send action; disconnecting\n", stderr);
      break;
    }
  }

  socket_cleanup(&socket_ctx);
  tcpme_quit();
  SDL_Quit();
  return 0;
}
