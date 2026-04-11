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
#include <SDL_net.h>

#include <pokeval.h>

#include "client.h"
#include "game.h"
#include "globals.h"
#include "net.h"
#include "types.h"

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

  /* Find quads, trips, pairs */
  int quad_face = -1, trip_face = -1;
  int pairs[2] = {-1, -1};
  int n_pairs = 0;

  for (int f = 1; f <= 14; f++) {
    if (face_count[f] == 4)
      quad_face = f;
    else if (face_count[f] == 3)
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

  /* Quads: stand pat (full_house already handled above via rank check, but
     let's be explicit in case of edge cases) */
  if (quad_face >= 0)
    return 0;

  /* Full house */
  if (trip_face >= 0 && n_pairs > 0)
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
          "\n"
          "Set DC_PASSWORD in the environment to authenticate with a\n"
          "password-protected server.\n",
          argv0, BOT_DEFAULT_PORT);
}

int main(int argc, char *argv[]) {
  const char *host = BOT_DEFAULT_HOST;
  uint16_t port = BOT_DEFAULT_PORT;
  const char *nick = BOT_DEFAULT_NICK;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--host") == 0 && i + 1 < argc)
      host = argv[++i];
    else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
      port = (uint16_t)atoi(argv[++i]);
    else if (strcmp(argv[i], "--nick") == 0 && i + 1 < argc)
      nick = argv[++i];
    else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      print_usage(argv[0]);
      return 1;
    }
  }

  pcg_srand_auto();

  if (SDL_Init(0) == -1 || SDLNet_Init() == -1) {
    fprintf(stderr, "SDL/SDLNet init failed: %s\n", SDL_GetError());
    return 1;
  }

  SocketContext_t socket_ctx = {0};
  const char *password = getenv("DC_PASSWORD");
  if (!bot_connect(host, port, nick, password ? password : "", &socket_ctx)) {
    fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
    SDLNet_Quit();
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
      SDLNet_Quit();
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

    /* Evaluate the current hand to decide whether to bet/raise.
     * Build a 5-card hand from however many valid cards we hold. */
    POKEVAL_Hand_5 h5 = {0};
    {
      int k5 = 0;
      for (int i = 0; i < MAX_HAND_SIZE && k5 < POKEVAL_HAND_SIZE; i++) {
        if (!DH_is_card_null(game_state.player[my_id].hand.card[i]))
          h5.card[k5++] = game_state.player[my_id].hand.card[i];
      }
    }
    hand_rank_t hand_rank = (hand_rank_t)POKEVAL_evaluate_hand(h5);
    bool strong_hand = (hand_rank > POKEVAL_TWO_PAIR);
    bool can_raise = (game_state.raises_remaining > 0);
    uint32_t bet_amount = game_settings.bet_amounts[0];

    /* Flags are set by the server sending the corresponding opcode directly to
       this client, so no need to check turn_id. */
    if (client_state.bet_check_fold) {
      /* With a strong hand, open the betting half the time; otherwise
         check most of the time and fold occasionally as a bluff/vary. */
      if (strong_hand && can_raise && pcg32_boundedrand_r(&rng, 2) == 0)
        send_player_action(&client_state, socket_ctx.sock, ACTION_BET, bet_amount);
      else if (pcg32_boundedrand_r(&rng, 100) < 15)
        send_player_action(&client_state, socket_ctx.sock, ACTION_FOLD, 0);
      else
        send_player_action(&client_state, socket_ctx.sock, ACTION_CHECK, 0);
      /* send_player_action clears bet_check_fold and call_raise_fold */
    } else if (client_state.call_raise_fold) {
      /* With a strong hand, raise half the time; otherwise call, with a small
         chance of a surprise fold to add unpredictability. */
      if (strong_hand && can_raise && pcg32_boundedrand_r(&rng, 2) == 0)
        send_player_action(&client_state, socket_ctx.sock, ACTION_RAISE, bet_amount);
      else if (pcg32_boundedrand_r(&rng, 100) < 10)
        send_player_action(&client_state, socket_ctx.sock, ACTION_FOLD, 0);
      else
        send_player_action(&client_state, socket_ctx.sock, ACTION_CALL, 0);
    } else if (client_state.call_complete_fold) {
      send_player_action(&client_state, socket_ctx.sock, ACTION_CALL, 0);
      client_state.call_complete_fold = false;
    } else if (client_state.complete_check_fold) {
      send_player_action(&client_state, socket_ctx.sock, ACTION_CHECK, 0);
      client_state.complete_check_fold = false;
    } else if (client_state.do_discard_draw) {
      uint8_t discard_indices[MAX_HAND_SIZE] = {0};
      uint8_t hand_size =
          client_state.game_choice ? client_state.game_choice->hand_size : POKEVAL_HAND_SIZE;
      bool lowball =
          (client_state.game_choice && client_state.game_choice->g == CALIFORNIA_LOWBALL);
      uint8_t n_discards =
          bot_choose_discards(&game_state.player[my_id].hand, hand_size, lowball, discard_indices);
      send_discards_request_new_cards(socket_ctx.sock, discard_indices, n_discards);
      client_state.do_discard_draw = false;
    }
  }

  socket_cleanup(&socket_ctx);
  SDLNet_Quit();
  SDL_Quit();
  return 0;
}
