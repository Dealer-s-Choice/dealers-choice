/*
 game_logic.c
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

/* The gameplay-screen render+input loop (handle_game_logic) and its private
 * card/coin/widget layout + rendering helpers.  Split out of client.c; the
 * cross-file helpers shared with client.c are declared in client_internal.h. */

#include <math.h>

#include <deckhandler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"
#include "client_internal.h"
#include "game.h"
#include "globals_gui.h"
#include "graphics.h"
#include "hotkey_overlay.h"
#include "hotkeys.h"
#include "widgets/button.h"
#include "widgets/card_text_atlas.h"
#include "widgets/dealer.h"
#include "widgets/image.h"
#include "widgets/indicator.h"
#include "widgets/nick.h"
#include "widgets/ping.h"
#include "widgets/step_scale.h"
#include "widgets/text.h"

#include "util.h"

#define FRAME_HITCH_WARN_MS 250 /* a frame gap >= this => UI stalled (#307) */
#define MAX_POT_COINS 60

static const uint8_t coin_px = 96;

/* Hand-rank overlay toggle (#61, default hotkey "h"): shows your current best
 * hand rank in the lower-right during a game. File-scope so the on/off state
 * PERSISTS across hands and across games (#88). handle_game_logic is re-entered
 * once per game from client.c's lobby loop, and hands cycle within a single
 * call, so a plain local would reset on both boundaries. This is a client-only
 * view preference, so it lives here rather than in the SDL-free, wire-protocol
 * ClientState_t. It is process-global by design: there is only ever one local
 * player, so a single static is the whole state. */
static bool g_show_hand_rank = false;

static void make_human_readable_card(DH_Card *card, CardWidget_t *cw) {
  const char *face = DH_get_card_face_str(card->face_val);
  const char *suit = DH_get_card_unicode_suit(*card);
  /* face_val + suit drive the card_text_atlas lookup in
   * card_widget_render — the actual TTF render happens once at
   * GUI startup, not per-frame. */
  cw->face_val = card->face_val;
  cw->suit = card->suit;
  snprintf(cw->text, sizeof(cw->text), "%s%s", face, suit);
  if (strlen(cw->text) == 0) {
    dc_log(DC_LOG_ERROR, "%s:String length 0", __func__);
    exit(EXIT_FAILURE);
  }
}

static void create_card_context(CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE],
                                const int start_i, Player_t *players_array,
                                const SDL_Point *player_pos, TTF_Font *font, const int my_id,
                                const bool deuces_wild) {
  memset(card_context, 0, sizeof(CardWidget_t) * MAX_PLAYERS * MAX_HAND_SIZE);
  if (start_i < 0 || start_i >= MAX_PLAYERS)
    return;
  Player_t *turn = &players_array[start_i];
  Player_t *starting_turn = turn;
  do {
    const int id = turn->id;
    if (id < 0 || id >= MAX_PLAYERS)
      break;
    for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
      // printf("%d\n", __LINE__);
      DH_Card *card = &(turn->hand.card)[card_n];
      const SDL_Point card_pos = {
          player_pos[id].x + card_n * (g_layout_cfg.card_w + g_layout_cfg.card_padding),
          player_pos[id].y,
      };

      CardWidget_t *cw = &card_context[id][card_n];
      card_widget_init(cw, font);
      cw->base.rect.x = card_pos.x;
      cw->base.rect.y = card_pos.y;
      cw->my_card = (id == my_id);

      cw->is_null = DH_is_card_null(*card);
      cw->is_back = DH_is_card_back(*card);

      if (!cw->is_back && !cw->is_null) {
        // Use a condition here so is_wild is not set to false if the card has
        // been changed
        if (!cw->is_wild && deuces_wild)
          cw->is_wild = card->face_val == DH_CARD_TWO;

        make_human_readable_card(card, cw);
      }
    }
    turn = get_next_connected_client(players_array, turn->id);
  } while (turn && turn != starting_turn);
}

/* After winner_declared: mark the 5 cards that form the winning hand.
 * For non-community games the server already trimmed to the best 5 (positions
 * 0-4, rest null), so every non-null card is a winning card.
 * For community card games (Hold'em, Omaha) we recompute the best hand on the
 * client and match cards back to their hand positions by face value and suit.
 * Community cards are marked on every player's context; layout_board_cards has
 * already set is_null=true on non-board player slots, so they won't render. */
static void mark_winning_cards(CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE],
                               Player_t *players_array, const GameChoice_t *game_choice,
                               bool deuces_wild) {
  if (!game_choice)
    return;

  bool is_community = false;
  for (int i = 0; i < MAX_HAND_SIZE; i++) {
    if (game_choice->card_slot[i] == CARD_SLOT_COMMUNITY) {
      is_community = true;
      break;
    }
  }

  for (int p = 0; p < MAX_PLAYERS; p++) {
    if (!players_array[p].is_connected || !players_array[p].winner)
      continue;

    if (!is_community) {
      /* Find the best 5-card hand from however many cards are held (may be
       * 5, 6, or 7 for stud games) and mark only those cards as winning. */
      POKEVAL_Hand_5 best = deuces_wild
                                ? POKEVAL_hand5_from_hand7_wild(&players_array[p].hand, DH_CARD_TWO)
                                : POKEVAL_hand5_from_hand7(&players_array[p].hand);
      for (int c = 0; c < MAX_HAND_SIZE; c++) {
        DH_Card card = players_array[p].hand.card[c];
        if (DH_is_card_null(card) || DH_is_card_back(card))
          continue;
        for (int b = 0; b < POKEVAL_HAND_SIZE; b++) {
          /* POKEVAL_sort_hand mutates Ace face_val from DH_CARD_ACE (1) to
           * POKEVAL_ACE (14) in place; normalise both sides before comparing
           * (handle_sort_hand on the server can also leave Aces at 14). */
          int32_t best_val =
              (best.card[b].face_val == POKEVAL_ACE) ? DH_CARD_ACE : best.card[b].face_val;
          int32_t card_val = (card.face_val == POKEVAL_ACE) ? DH_CARD_ACE : card.face_val;
          if (card_val == best_val && card.suit == best.card[b].suit) {
            card_context[p][c].is_winning = true;
            break;
          }
        }
      }
      continue;
    }

    // Recompute the best 5-card hand for this winner
    POKEVAL_Hand_5 best;
    if (game_choice->game_type == game_choices[OMAHA].game_type)
      best = POKEVAL_hand5_omaha(&players_array[p].hand);
    else
      best = POKEVAL_hand5_from_hand7(&players_array[p].hand);

    for (int c = 0; c < MAX_HAND_SIZE; c++) {
      DH_Card card = players_array[p].hand.card[c];
      if (DH_is_card_null(card) || DH_is_card_back(card))
        continue;
      for (int b = 0; b < POKEVAL_HAND_SIZE; b++) {
        int32_t best_val =
            (best.card[b].face_val == POKEVAL_ACE) ? DH_CARD_ACE : best.card[b].face_val;
        int32_t card_val = (card.face_val == POKEVAL_ACE) ? DH_CARD_ACE : card.face_val;
        if (card_val == best_val && card.suit == best.card[b].suit) {
          if (game_choice->card_slot[c] == CARD_SLOT_HOLE) {
            card_context[p][c].is_winning = true;
          } else {
            // Community card: mark on all players; non-board slots are is_null so won't render
            for (int q = 0; q < MAX_PLAYERS; q++)
              card_context[q][c].is_winning = true;
          }
          break;
        }
      }
    }
  }
}

/* Reposition community cards to a centered row at the top of the table and
 * suppress those positions from all other player hand slots.
 * community_start is the first card index that is a community card. */
static void layout_board_cards(CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE],
                               const int board_player_id, const int community_start,
                               const int community_count) {
  const int row_w =
      community_count * g_layout_cfg.card_w + (community_count - 1) * g_layout_cfg.card_padding;
  const int board_x = g_center.x - row_w / 2;
  const int board_y = g_viewport.y + g_layout_cfg.community_top_offset;

  for (int card_n = community_start; card_n < community_start + community_count; card_n++) {
    int slot = card_n - community_start;
    SDL_Rect rect = {board_x + slot * (g_layout_cfg.card_w + g_layout_cfg.card_padding), board_y,
                     g_layout_cfg.card_w, g_layout_cfg.card_h};
    card_context[board_player_id][card_n].base.rect = rect;

    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (i == board_player_id)
        continue;
      card_context[i][card_n].is_null = true;
    }
  }
}

void layout_cards(CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE], Player_t *players_array,
                  const SDL_Point *player_pos) {

  Player_t *starting_turn = NULL;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (players_array[i].is_connected) {
      starting_turn = &players_array[i];
      break;
    }
  }
  if (!starting_turn)
    return;
  Player_t *turn = starting_turn;
  do {
    for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
      const int id = turn->id;
      SDL_Rect rect = {player_pos[id].x +
                           card_n * (g_layout_cfg.card_w + g_layout_cfg.card_padding),
                       player_pos[id].y, g_layout_cfg.card_w, g_layout_cfg.card_h};
      card_context[id][card_n].base.rect = rect;
    }
    turn = get_next_connected_client(players_array, turn->id);
  } while (turn && turn != starting_turn);
}

typedef struct {
  SDL_Texture *texture;
  SDL_Point start;
  SDL_Point end;
  uint32_t start_time;
  uint32_t duration; // in milliseconds
  bool active;
} CoinAnimation_t;

static void render_coin_animation(SDL_Renderer *renderer, CoinAnimation_t *anim) {
  if (!anim->active) {
    return;
  }

  if (anim->duration <= 0) {
    anim->active = false;
    return;
  }

  uint32_t now = SDL_GetTicks();
  float progress = (now - anim->start_time) / (float)anim->duration;

  if (progress >= 1.0f) {
    progress = 1.0f;
    anim->active = false;
  }

  float fx = anim->start.x + (anim->end.x - anim->start.x) * progress;
  float fy = anim->start.y + (anim->end.y - anim->start.y) * progress;

  SDL_Rect dst = {.x = (int)fx, .y = (int)fy, .w = coin_px, .h = coin_px};

  SDL_RenderCopyEx(renderer, anim->texture, NULL, &dst, progress * 720.0, NULL, SDL_FLIP_NONE);
}

enum {
  CHECK,
  BET,
  FOLD,
  CALL,
  RAISE,
  COMPLETE,
  DISCARD,
  MAX_ACTIONS,
};

typedef struct {
  const char *text;
  const char *past_tense; /* announced to other players when this action is taken (#74) */
} ActionButtonAttrs;

static const ActionButtonAttrs action_button_attrs[MAX_ACTIONS] = {
    [CHECK] = {N_("Check"), N_("checked")},  [BET] = {N_("Bet"), N_("bet")},
    [FOLD] = {N_("Fold"), N_("folded")},     [CALL] = {N_("Call"), N_("called")},
    [RAISE] = {N_("Raise"), N_("raised")},   [COMPLETE] = {N_("Complete"), N_("completed")},
    [DISCARD] = {N_("Discard"), N_("drew")},
};

/* Map a wire announce verb (EActionAnnounce_t) to its action-button index, so the
 * client renders the action in its own locale instead of the server's (#74). */
static int action_button_for_verb(int verb) {
  switch (verb) {
  case ANNOUNCE_CHECKED:
    return CHECK;
  case ANNOUNCE_BET:
    return BET;
  case ANNOUNCE_FOLDED:
    return FOLD;
  case ANNOUNCE_CALLED:
    return CALL;
  case ANNOUNCE_RAISED:
    return RAISE;
  case ANNOUNCE_COMPLETED:
    return COMPLETE;
  case ANNOUNCE_DREW:
    return DISCARD;
  default:
    return -1;
  }
}

/* Best 5-card rank name for the local player's current hand, or NULL if fewer
 * than 5 cards are dealt yet. Drives the CTRL-H rank readout (#61). */
static const char *local_hand_rank_name(const POKEVAL_Hand_9 *hand, const GameChoice_t *choice,
                                        bool deuces_wild) {
  size_t n = 0;
  while (n < 9 && !DH_is_card_null(hand->card[n]))
    n++;

  short r;
  if (choice && choice->g == OMAHA) {
    /* Omaha uses exactly 2 hole + 3 board; POKEVAL_hand5_omaha iterates all 5
     * community slots, so only evaluate once the full board (4 hole + 5
     * community = 9 cards) is dealt — otherwise it would fold null slots in. */
    if (n < 9)
      return NULL;
    r = POKEVAL_evaluate_hand(POKEVAL_hand5_omaha(hand));
  } else {
    if (n < POKEVAL_HAND_SIZE)
      return NULL;
    if (deuces_wild)
      r = POKEVAL_evaluate_hand_wild(POKEVAL_hand5_from_hand7_wild(hand, DH_CARD_TWO), DH_CARD_TWO);
    else
      r = POKEVAL_evaluate_hand(POKEVAL_hand5_from_hand7(hand));
  }
  if (r < 0 || r >= NUM_HAND_RANKS)
    return NULL;
  return POKEVAL_rank[r];
}

static SDL_Keycode action_hotkey(int action) {
  switch (action) {
  case CHECK:
    return g_hotkey_cfg.check;
  case BET:
    return g_hotkey_cfg.bet;
  case FOLD:
    return g_hotkey_cfg.fold;
  case CALL:
    return g_hotkey_cfg.call;
  case RAISE:
    return g_hotkey_cfg.raise;
  case COMPLETE:
    return g_hotkey_cfg.complete;
  case DISCARD:
    return g_hotkey_cfg.discard;
  default:
    return SDLK_UNKNOWN;
  }
}

/* The F1 "Keys" reference overlay lives in hotkey_overlay.c now, shared with the
 * connect screen and the lobby (see hotkey_overlay_render / _handle_event). */

static void layout_action_buttons(ButtonWidget_t **b) {
  for (int i = 0; i < MAX_ACTIONS; i++) {
    b[i]->base.rect.y = g_viewport.h - (b[i]->base.rect.h * 6);
  }
}

typedef struct {
  SDL_Point offset;
  SDL_Rect rect;
  double angle;
} CoinInPot_t;

static void layout_coins(CoinInPot_t *coins, SDL_Point *p, int count) {
  for (int i = 0; i < count; i++) {
    coins[i].rect.w = coin_px;
    coins[i].rect.h = coin_px;

    coins[i].rect.x = p->x + coins[i].offset.x - coin_px / 2;
    coins[i].rect.y = p->y + coins[i].offset.y - coin_px / 2;
  }
}

static void layout_indicator(Indicator_t *ind, int x, int y) {
  ui_widget_place(&ind->base, x, y); // sets base.rect.x/y

  ind->rx = ind->base.rect.w / 2;
  ind->ry = ind->base.rect.h / 2;

  ind->cx = ind->base.rect.x + ind->rx;
  ind->cy = ind->base.rect.y + ind->ry;
}

/* Dark-green 3D bevel bar — a dashboard cell / row / inter-button divider. */
static void draw_dash_divider(SDL_Renderer *r, SDL_Rect d) {
  SDL_Color b = DC_DASH_DIVIDER;
  SDL_SetRenderDrawColor(r, b.r, b.g, b.b, 255);
  SDL_RenderFillRect(r, &d);
  SDL_SetRenderDrawColor(r, (Uint8)(b.r + 40), (Uint8)(b.g + 55), (Uint8)(b.b + 40), 255);
  SDL_RenderDrawLine(r, d.x, d.y, d.x + d.w - 1, d.y); /* top    (light) */
  SDL_RenderDrawLine(r, d.x, d.y, d.x, d.y + d.h - 1); /* left   (light) */
  SDL_SetRenderDrawColor(r, (Uint8)(b.r > 12 ? b.r - 12 : 0), (Uint8)(b.g > 35 ? b.g - 35 : 0),
                         (Uint8)(b.b > 18 ? b.b - 18 : 0), 255);
  SDL_RenderDrawLine(r, d.x, d.y + d.h - 1, d.x + d.w - 1, d.y + d.h - 1); /* bottom (dark) */
  SDL_RenderDrawLine(r, d.x + d.w - 1, d.y, d.x + d.w - 1, d.y + d.h - 1); /* right  (dark) */
}

static DC_NOINLINE void draw_filled_circle(SDL_Renderer *r, int cx, int cy, int radius) {
  if (radius <= 0)
    return;

  for (int y = -radius; y <= radius; y++) {
    float dy = (float)y / (float)radius;
    float inside = 1.0f - dy * dy;
    if (inside < 0.0f)
      continue;

    int x = (int)(radius * sqrtf(inside) + 0.5f);
    SDL_RenderDrawLine(r, cx - x, cy + y, cx + x, cy + y);
  }
}

/* Circle timer ------------------------------------------------------------- */

/*
 * render_circle_timer - draw a 100px-diameter pie-style countdown circle.
 *
 * fill_ratio : 1.0 = full (just started, no fill), 0.0 = empty (all filled)
 *
 * The brown wedge grows clockwise from 12 o'clock as time elapses.
 * A 10px 3D border ring (light on top-left, dark on bottom-right) frames it.
 */
static void render_circle_timer(SDL_Renderer *renderer, SDL_Point center, float fill_ratio,
                                bool my_turn) {
  const int cx = center.x;
  const int cy = center.y;
  const int outer_r = g_layout_cfg.circle_timer_r; /* 50 */
  const int border = g_layout_cfg.timer_border;
  const int inner_r = outer_r - border; /* 40 */

  /* --- 3D border ring ---------------------------------------------------- */
  for (int y = cy - outer_r; y <= cy + outer_r; y++) {
    int dy = y - cy;
    float dy_f = (float)dy;
    int outer_hw = (int)sqrtf((float)(outer_r * outer_r) - dy_f * dy_f);
    float inner_r2 = (float)(inner_r * inner_r);
    int inner_hw = (dy_f * dy_f <= inner_r2) ? (int)sqrtf(inner_r2 - dy_f * dy_f) : -1;

    for (int x = cx - outer_hw; x <= cx + outer_hw; x++) {
      int dx = x - cx;
      /* skip pixels inside the inner circle */
      if (inner_hw >= 0 && dx >= -inner_hw && dx <= inner_hw)
        continue;
      /* 3D shading: dot with (-1,-1) direction; bright top-left, dark bottom-right */
      float t = (-(float)dx - dy_f) / ((float)outer_r * 1.414f);
      if (t > 1.0f)
        t = 1.0f;
      if (t < -1.0f)
        t = -1.0f;
      t = (t + 1.0f) * 0.5f;                     /* 0..1 */
      uint8_t c = (uint8_t)(70.0f + t * 185.0f); /* 70 (dark gray) → 255 (white) */
      SDL_SetRenderDrawColor(renderer, c, c, c, 255);
      SDL_RenderDrawPoint(renderer, x, y);
    }
  }

  /* --- inner circle background ------------------------------------------- */
  SDL_Color tbg = DC_TIMER_BG;
  SDL_SetRenderDrawColor(renderer, tbg.r, tbg.g, tbg.b, tbg.a);
  for (int y = cy - inner_r; y <= cy + inner_r; y++) {
    int dy = y - cy;
    int hw = (int)sqrtf((float)(inner_r * inner_r - dy * dy));
    SDL_RenderDrawLine(renderer, cx - hw, y, cx + hw, y);
  }

  /* --- pie wedge (elapsed time, clockwise from 12 o'clock) --------------- */
  float sweep = (1.0f - fill_ratio) * 2.0f * (float)M_PI;
  if (sweep > 0.001f) {
    SDL_Color te = my_turn ? DC_TIMER_ELAPSED : (SDL_Color){128, 0, 0, 255};
    SDL_SetRenderDrawColor(renderer, te.r, te.g, te.b, te.a);
    for (int y = cy - inner_r; y <= cy + inner_r; y++) {
      int dy = y - cy;
      int hw = (int)sqrtf((float)(inner_r * inner_r - dy * dy));
      for (int x = cx - hw; x <= cx + hw; x++) {
        int dx = x - cx;
        /* angle from 12 o'clock, increasing clockwise */
        float a = atan2f((float)dx, -(float)dy);
        if (a < 0.0f)
          a += 2.0f * (float)M_PI;
        if (a <= sweep)
          SDL_RenderDrawPoint(renderer, x, y);
      }
    }
  }
}
/* end circle timer --------------------------------------------------------- */

/*
 * word_wrap_status - break `text` into lines that fit within `max_w` pixels
 * using `font`.  Writes at most `max_lines` NUL-terminated strings into `out`
 * (each slot is LEN_STATUS_STR bytes).  Returns the number of lines produced.
 *
 * Words are split on spaces.  A single word that is wider than `max_w` is
 * placed on its own line anyway (no mid-word break at this resolution).
 */
static int word_wrap_status(TTF_Font *font, const char *text, int max_w, char out[][LEN_STATUS_STR],
                            int max_lines) {
  int n = 0;
  const char *p = text;

  while (*p && n < max_lines) {
    const char *line_start = p;
    const char *last_break = p; /* end of last word-run that still fits */

    while (*p) {
      /* skip whitespace then scan one word */
      const char *ws = p;
      while (*ws == ' ')
        ws++;
      const char *we = ws;
      while (*we && *we != ' ')
        we++;

      /* measure from line_start to end of this word */
      int span = (int)(we - line_start);
      if (span >= LEN_STATUS_STR)
        span = LEN_STATUS_STR - 1;

      char tmp[LEN_STATUS_STR];
      memcpy(tmp, line_start, (size_t)span);
      tmp[span] = '\0';

      int w = 0;
      TTF_SizeUTF8(font, tmp, &w, NULL);

      if (w <= max_w || last_break == line_start) {
        /* fits (or it's the very first word — always accept it) */
        last_break = we;
        p = we;
        if (!*p)
          break;
      } else {
        /* this word pushes over; wrap before it */
        break;
      }
    }

    /* copy [line_start, last_break) trimming trailing spaces */
    int len = (int)(last_break - line_start);
    while (len > 0 && line_start[len - 1] == ' ')
      len--;
    if (len >= LEN_STATUS_STR)
      len = LEN_STATUS_STR - 1;
    memcpy(out[n], line_start, (size_t)len);
    out[n][len] = '\0';
    n++;

    /* advance past whitespace before the next line */
    p = last_break;
    while (*p == ' ')
      p++;
  }

  if (n == 0) {
    out[0][0] = '\0';
    n = 1;
  }

  return n;
}

static void render_text_pot(const char *text, const SDL_Point center, const Font_t *font) {
  if (!text)
    text = "";

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font->fonts[FONT_DEFAULT_BOLD], *text ? text : " ",
                                                get_color(COLOR_BLACK));
  if (!surface) {
    dc_log(DC_LOG_ERROR, "TTF_RenderUTF8_Blended error: %s", TTF_GetError());
    return;
  }

  SDL_Renderer *renderer = g_sdl_context->renderer;
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    SDL_FreeSurface(surface);
    dc_log(DC_LOG_ERROR, "SDL_CreateTextureFromSurface error: %s", SDL_GetError());
    return;
  }

  int text_w = surface->w;
  int text_h = surface->h;

  /* Background circle */
  int PAD = g_layout_cfg.indicator_pad;
  int radius = (text_w > text_h ? text_w : text_h) / 2 + PAD;
  if (radius < g_layout_cfg.indicator_min_r)
    radius = 24;

  SDL_SetRenderDrawBlendMode(g_sdl_context->renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
  draw_filled_circle(renderer, center.x, center.y, radius);

  /* Text rect centered */
  SDL_Rect text_rect = {center.x - text_w / 2, center.y - text_h / 2, text_w, text_h};

  SDL_RenderCopy(renderer, texture, NULL, &text_rect);

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}
EGameLogicResult_t handle_game_logic(const PlayerConfig_t *player_config,
                                     SocketContext_t *socket_context,
                                     const GameSettings_t *game_settings, GameState_t *game_state,
                                     SdlContext_t *sdl_context, const Font_t *font, Path_t *path,
                                     const SoundContext_t *sound_context,
                                     SDL_Texture **coin_textures, size_t n_coin_images) {
  card_widget_select_back_for_game();

  ClientState_t client_state = {0};

  NickWidget_t *game_nick_widgets[MAX_PLAYERS] = {0};
  ImageWidget_t *game_coin_widgets[MAX_PLAYERS] = {0};
  TextWidget_t *game_coins_tw[MAX_PLAYERS] = {0};
  int8_t selected_nick = -1;

  bool disconnected = false;
  bool was_connected[MAX_PLAYERS];
  int32_t prev_coins[MAX_PLAYERS];
  bool was_in[MAX_PLAYERS];
  POKEVAL_Hand_9 preserved_hands[MAX_PLAYERS];
  bool folded_showing[MAX_PLAYERS];
  int last_bettor_id = 0;
  for (int i = 0; i < MAX_PLAYERS; i++) {
    was_connected[i] = game_state->player[i].is_connected;
    prev_coins[i] = game_state->player[i].coins;
    was_in[i] = game_state->player[i].in;
    preserved_hands[i] = game_state->player[i].hand;
    folded_showing[i] = false;
  }

  char status_msgs[SIZEOF_STATUS_MSGS][LEN_STATUS_STR] = {0};
  char last_status_str[LEN_STATUS_STR] = {0};
  TextWidget_t *status_tw[SIZEOF_STATUS_MSGS] = {0};
  TextWidget_t *hand_rank_tw = text_widget_create(" ", font->fonts[FONT_BOLD], DC_TEXT_ON_DARK);

  ButtonWidget_t *action_bw[MAX_ACTIONS];
  for (int i = 0; i < MAX_ACTIONS; i++) {
    action_bw[i] = button_widget_create_styled(action_button_attrs[i].text, &ROLE_PRIMARY,
                                               font->fonts, action_hotkey(i));
  }
  layout_action_buttons(action_bw);

  TextWidget_t *discard_hint_tw = text_widget_create("You may only discard a maximum of 3 cards",
                                                     font->fonts[FONT_BOLD], DC_DISCARD_TEXT);
  uint8_t last_max_allowed = 3;

  /* Per-seat glyph marking who opens the current betting round; sits in the
   * nameplate's left padding, just left of the nick. */
  TextWidget_t *opener_tag =
      text_widget_create("{}", font->fonts[FONT_BOLD], (SDL_Color){255, 140, 0, 255});

  static const SDL_Keycode bet_hotkeys[MAX_BET_AMOUNTS] = {
      SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8,
  };
  const size_t n_bet_amounts = game_settings->bet_amount_count;

  /* Bet amount is chosen on a notched step scale (widgets/step_scale.c). */
  StepScaleWidget_t *bet_scale = step_scale_create(game_settings->bet_amounts, bet_hotkeys,
                                                   (int)n_bet_amounts, font->fonts[FONT_CARD]);
  if (n_bet_amounts > 0)
    client_state.selected_amount = game_settings->bet_amounts[0];

  CardWidget_t card_context[MAX_PLAYERS][MAX_HAND_SIZE];

  UIRegistry_t registry = {0};

  ButtonWidget_t *game_btn_kick =
      button_widget_create_styled(_("Kick"), &ROLE_ALT, font->fonts, (SDL_Keycode)0);
  ButtonWidget_t *game_btn_ban =
      button_widget_create_styled(_("Ban"), &ROLE_ALT, font->fonts, (SDL_Keycode)0);
  ButtonWidget_t *game_btn_quit =
      button_widget_create_styled("X", &ROLE_DANGER, font->fonts, (SDL_Keycode)0);
  game_btn_kick->base.rect.x = g_viewport.w / 10;
  /* Position above the status message panel, which starts at g_center.y */
  game_btn_kick->base.rect.y =
      g_center.y - game_btn_kick->base.rect.h - g_layout_cfg.game_kick_y_gap;
  game_btn_ban->base.rect.x =
      game_btn_kick->base.rect.x + game_btn_kick->base.rect.w + g_layout_cfg.kick_ban_btn_gap;
  game_btn_ban->base.rect.y = game_btn_kick->base.rect.y;
  game_btn_quit->base.rect.x =
      g_viewport.x + g_viewport.w - game_btn_quit->base.rect.w - g_layout_cfg.margin;
  game_btn_quit->base.rect.y = g_viewport.y + g_layout_cfg.margin;
  ui_register(&registry, &game_btn_kick->base);
  ui_register(&registry, &game_btn_ban->base);
  ui_register(&registry, &game_btn_quit->base);

  Indicator_t *indicator_deuces_wild = create_indicator_colored(
      _("Deuces Wild"), font->fonts[FONT_BOLD], DC_INDICATOR_WILD_BG, DC_INDICATOR_WILD_FG);
  ui_register(&registry, &indicator_deuces_wild->base);
  indicator_deuces_wild->base.enabled = false; /* drawn by the dashboard, not ui_render_all */

  Indicator_t *indicator_game_name = NULL;

  /* Static width for the game-name indicator cell, sized to the LONGEST game
   * name so the T's two cells don't resize per game (mirrors the indicator's own
   * text_w + 2*text_h sizing). */
  int static_gamename_w = 0;
  {
    int th = 0;
    for (int gi = 0; gi < MAX_CHOICES; gi++) {
      int tw = 0;
      if (TTF_SizeUTF8(font->fonts[FONT_BOLD], game_choices[gi].str, &tw, &th) == 0 &&
          tw > static_gamename_w)
        static_gamename_w = tw;
    }
    static_gamename_w += 2 * th;
  }

  int running = 1;
  bool cards_created = false;
  bool winner_highlighted = false;
  /* g_show_hand_rank (file-scope) holds the hand-rank overlay toggle; it
   * persists across hands and games (#88), so it is not declared here. */
  bool show_hotkey_overlay = false; /* F1 reference panel; non-blocking */

  Player_t *players_array = game_state->player;
  Player_t *turn = NULL;
  Player_t *starting_turn = NULL;

  const size_t which_coin = n_coin_images > 0 ? pcg32_boundedrand_r(&rng, n_coin_images) : 0;
  SDL_Texture *coin_tex_front = n_coin_images > 0 ? coin_textures[which_coin] : NULL;

  SDL_Texture *felt_tex =
      load_coin_texture(sdl_context->renderer, path->data, "100x100-green-felt-seamless-tile.png");
  SDL_Texture *vignette_tex = create_vignette_texture(sdl_context->renderer);

  TextWidget_t *open_seat_tw[MAX_PLAYERS] = {0};
  for (int i = 0; i < MAX_PLAYERS; i++)
    open_seat_tw[i] = text_widget_create("Open", font->fonts[FONT_DEFAULT], DC_TEXT_MUTED);

  CoinInPot_t coin_in_pot[MAX_POT_COINS] = {0};

  uint8_t coins = 0;
  CoinAnimation_t coin_anim = {0};
  bool prev_winner_declared = game_state->winner_declared;

  for (int i = 0; i < SIZEOF_STATUS_MSGS; i++) {
    status_tw[i] = text_widget_create(" ", font->fonts[FONT_STATUS_MSG], DC_TEXT_ON_LIGHT);
    if (status_tw[i])
      ui_widget_place(&status_tw[i]->base, g_layout.msg_panel.x + g_layout_cfg.msg_panel_pad_x,
                      g_layout.msg_panel.y + g_layout_cfg.msg_panel_pad_y +
                          i * (g_layout.status_line_h + 2));
  }

  client_state.timer_start = SDL_GetTicks();
  client_state.hourglass_rotate_start = client_state.timer_start;

  int8_t my_id = game_settings->client_id;
  if (my_id < 0 || my_id >= MAX_PLAYERS) {
    dc_log(DC_LOG_WARN, "handle_game_logic: client_id %d out of range", my_id);
    my_id = 0;
  }
  tcpme_socket_t sock = socket_context->sock;

  /* Frame-hitch detection: the loop renders continuously, so a large gap
   * between consecutive frame starts means the main thread stalled (a freeze).
   * Catches network/audio/render blocks the server can't see. (#307) */
  uint32_t dc_last_frame = SDL_GetTicks();

  while (running) {
    uint32_t dc_frame_now = SDL_GetTicks();
    uint32_t dc_frame_dt = dc_frame_now - dc_last_frame;
    if (dc_frame_dt >= FRAME_HITCH_WARN_MS)
      dc_log(DC_LOG_WARN, "frame stall: %ums (client UI was unresponsive)", dc_frame_dt);
    dc_last_frame = dc_frame_now;

    POKEVAL_Hand_9 prev_hands[MAX_PLAYERS];
    bool prev_connected[MAX_PLAYERS];
    bool prev_in[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; i++) {
      prev_hands[i] = game_state->player[i].hand;
      prev_connected[i] = game_state->player[i].is_connected;
      prev_in[i] = game_state->player[i].in;
    }

    ERecvStatus_t recv_status = recv_game_state(socket_context, game_state, &client_state, my_id);
    // printf("%d\n", __LINE__);
    if (recv_status == RECV_ERROR) {
      disconnected = true;
      running = false;
    } else if (recv_status == RECV_SUCCESS) {
      for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game_state->player[i].in)
          folded_showing[i] = false;
        if (was_in[i] && !game_state->player[i].in) {
          preserved_hands[i] = prev_hands[i];
          folded_showing[i] = true;
        }
        if (folded_showing[i] && game_state->player[i].is_connected &&
            !game_state->player[i].in)
          game_state->player[i].hand = preserved_hands[i];
        was_in[i] = game_state->player[i].in;
      }
      /* Only rebuild the card widgets when something they depend on changed
       * (hands, seating, or fold-state); bet/turn/pot/coin updates don't. */
      bool cards_dirty = false;
      for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game_state->player[i].is_connected != prev_connected[i] ||
            game_state->player[i].in != prev_in[i] ||
            memcmp(&game_state->player[i].hand, &prev_hands[i], sizeof(POKEVAL_Hand_9)) != 0) {
          cards_dirty = true;
          break;
        }
      }
      if (!game_state->winner_declared) {
        if (cards_dirty)
          cards_created = false;
        winner_highlighted = false;
      } else if (!winner_highlighted) {
        cards_created = false;
      }

      /* Render a player's action in our own locale from the wire code (#74). */
      if (client_state.action_announce_pending) {
        client_state.action_announce_pending = false;
        int bidx = action_button_for_verb(client_state.action_announce_verb);
        int8_t aid = client_state.action_announce_player;
        if (bidx >= 0 && aid >= 0 && aid < MAX_PLAYERS) {
          const char *verb_str = _(action_button_attrs[bidx].past_tense);
          const char *nick = game_state->player[aid].nick;
          /* DREW always shows its count, even 0 ("drew 0"): the number is the
           * card-draw count, not a chip amount, so suppressing it on 0 (as we do
           * for bet/call/raise/complete) would read just "drew". This regressed
           * in the #74 move of status assembly from server to client, where the
           * server's unconditional "%s drew %d" became the shared amount>0 guard. */
          if (client_state.action_announce_verb == ANNOUNCE_DREW ||
              client_state.action_announce_amount > 0)
            snprintf(client_state.server_status_str, LEN_STATUS_STR, "%s %s %u", nick, verb_str,
                     client_state.action_announce_amount);
          else
            snprintf(client_state.server_status_str, LEN_STATUS_STR, "%s %s", nick, verb_str);
        }
      }
    }

    if (game_state->at_menu)
      break;

    int8_t *turn_id = &client_state.turn_id;
    if (*turn_id < 0 || *turn_id >= MAX_PLAYERS)
      *turn_id = 0;

    if (!starting_turn)
      starting_turn = &game_state->player[*turn_id];

    // For cases when the client who was designated as starting_turn disconnects
    // or folds (server nulls all their cards including community slots).
    if (!starting_turn->is_connected || !starting_turn->in) {
      starting_turn = get_next_player(players_array, starting_turn->id);
      if (!starting_turn) {
        disconnected = true;
        running = false;
        break;
      }
    }

    turn = &game_state->player[*turn_id];

    bool joined[MAX_PLAYERS] = {0};
    bool left[MAX_PLAYERS] = {0};
    detect_player_changes(game_state, was_connected, joined, left);

    for (int8_t id = 0; id < MAX_PLAYERS; id++) {
      if (left[id]) {
        if (selected_nick == id)
          selected_nick = -1;
        if (game_nick_widgets[id]) {
          ui_unregister(&registry, &game_nick_widgets[id]->base);
          ui_widget_destroy(&game_nick_widgets[id]->base);
          game_nick_widgets[id] = NULL;
        }
        if (game_coin_widgets[id]) {
          ui_unregister(&registry, &game_coin_widgets[id]->base);
          ui_widget_destroy(&game_coin_widgets[id]->base);
          game_coin_widgets[id] = NULL;
        }
        if (game_coins_tw[id]) {
          ui_unregister(&registry, &game_coins_tw[id]->base);
          ui_widget_destroy(&game_coins_tw[id]->base);
          game_coins_tw[id] = NULL;
        }
      }
      if (!game_state->player[id].is_connected || game_nick_widgets[id])
        continue;
      char coins_str[24] = {0};
      snprintf(coins_str, sizeof coins_str, "%" PRId32, game_state->player[id].coins);
      game_nick_widgets[id] = nick_widget_create(game_state->player[id].nick, id,
                                                 font->fonts[FONT_BOLD], DC_TEXT_ON_DARK);
      game_nick_widgets[id]->highlight = (id == my_id);
      game_nick_widgets[id]->selectable = (game_state->player[my_id].is_admin && id != my_id);
      game_coin_widgets[id] = image_widget_from_texture(coin_tex_front, coin_px / 2, coin_px / 2);
      game_coins_tw[id] = text_widget_create(coins_str, font->fonts[FONT_BOLD], DC_TEXT_ON_DARK);
      ui_register(&registry, &game_nick_widgets[id]->base);
      ui_register(&registry, &game_coin_widgets[id]->base);
      ui_register(&registry, &game_coins_tw[id]->base);
      prev_coins[id] = game_state->player[id].coins;
    }
    // printf("turn id: %d\n", turn->id);

    if (strcmp(client_state.server_status_str, last_status_str) != 0) {
      snprintf(last_status_str, LEN_STATUS_STR, "%s", client_state.server_status_str);

      int max_w = g_layout.msg_panel.w - g_layout_cfg.msg_panel_pad_x * 2;
      char wrapped[SIZEOF_STATUS_MSGS][LEN_STATUS_STR];
      int n = word_wrap_status(font->fonts[FONT_STATUS_MSG], client_state.server_status_str, max_w,
                               wrapped, SIZEOF_STATUS_MSGS);

      /* Shift existing lines up by n to make room at the bottom */
      memmove(status_msgs, status_msgs + n, sizeof(status_msgs[0]) * (SIZEOF_STATUS_MSGS - n));
      for (int i = 0; i < n; i++)
        memcpy(status_msgs[SIZEOF_STATUS_MSGS - n + i], wrapped[i], LEN_STATUS_STR);

      for (int i = 0; i < SIZEOF_STATUS_MSGS; i++)
        text_widget_set_text(status_tw[i], *status_msgs[i] ? status_msgs[i] : " ");
    }

    // debug_print_cards(&game_state->player[0].hand);
    // debug_print_cards(&game_state->player[1].hand);

    // Only create the cards but doesn't render them yet.
    // This is done when the client receives new data, not every iteration
    // of the loop. The flag gets set above. TODO: It should only happen if
    // the server sends new information about cards.

    /* Rotate seats so the local player is bottom-center and opponents fill the
     * fixed ring clockwise. Built each frame; used in place of g_layout.player_pos
     * by every per-player position below. The local row is centered on its own
     * card count (hole cards; community slots are repositioned to the board). */
    SDL_Point seat_pos[MAX_PLAYERS];
    int local_row_w;
    {
      int n_local = 0;
      for (int i = 0; i < MAX_HAND_SIZE; i++) {
        if (client_state.game_choice &&
            client_state.game_choice->card_slot[i] == CARD_SLOT_COMMUNITY)
          continue;
        if (!DH_is_card_null(game_state->player[my_id].hand.card[i]))
          n_local++;
      }
      if (n_local < 1)
        n_local = 2;
      local_row_w = n_local * g_layout_cfg.card_w + (n_local - 1) * g_layout_cfg.card_padding;
      layout_seats_for(seat_pos, my_id, local_row_w);
    }

    /* First community-card slot for this game (-1 if none); drives both the board
     * layout and the placeholder shadows below. */
    int community_start = -1, community_count = 0;
    if (client_state.game_choice)
      for (int cs = 0; cs < MAX_HAND_SIZE; cs++)
        if (client_state.game_choice->card_slot[cs] == CARD_SLOT_COMMUNITY) {
          if (community_start < 0)
            community_start = cs;
          community_count++;
        }

    if (!cards_created) {
      // printf("%d\n", __LINE__);
      create_card_context(card_context, starting_turn->id, players_array, seat_pos,
                          font->fonts[FONT_CARD], my_id, client_state.deuces_wild);
      layout_cards(card_context, players_array, seat_pos);

      /* Dim the local player's own hole cards (private to opponents), but only
       * in variants that also have visible cards (stud/holdem), so a pure-draw
       * hand isn't uniformly shaded (#64). */
      if (client_state.game_choice && my_id >= 0 && my_id < MAX_PLAYERS) {
        const CardSlotType_t *slot = client_state.game_choice->card_slot;
        bool has_visible = false;
        for (int k = 0; k < MAX_HAND_SIZE; k++)
          if (slot[k] == CARD_SLOT_FACE_UP || slot[k] == CARD_SLOT_COMMUNITY)
            has_visible = true;
        if (has_visible)
          for (int k = 0; k < MAX_HAND_SIZE; k++)
            card_context[my_id][k].is_shaded = (slot[k] == CARD_SLOT_HOLE);
      }

      if (community_start > 0)
        layout_board_cards(card_context, starting_turn->id, community_start, community_count);
      if (game_state->winner_declared) {
        if (game_state->player_count > 1)
          mark_winning_cards(card_context, game_state->player, client_state.game_choice,
                             client_state.deuces_wild);
        winner_highlighted = true;
      }
      cards_created = true;
    }

    clear_screen(sdl_context->renderer);
    draw_felt_background(sdl_context->renderer, felt_tex);
    if (vignette_tex)
      SDL_RenderCopy(sdl_context->renderer, vignette_tex, NULL, NULL);

    int mx, my;
    SDL_GetMouseState(&mx, &my);
    float lx, ly;
    SDL_RenderWindowToLogical(sdl_context->renderer, mx, my, &lx, &ly);
    SDL_Point mouse_pos = {(int)lx, (int)ly};

    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (game_nick_widgets[i])
        game_nick_widgets[i]->base.hovered =
            game_nick_widgets[i]->selectable &&
            SDL_PointInRect(&mouse_pos, &game_nick_widgets[i]->base.rect);
    }

    bool game_admin = game_state->player[my_id].is_admin;
    game_btn_kick->base.enabled = game_admin && selected_nick >= 0;
    game_btn_kick->interactive = game_admin && selected_nick >= 0;
    game_btn_kick->base.hovered =
        game_btn_kick->interactive && SDL_PointInRect(&mouse_pos, &game_btn_kick->base.rect);
    game_btn_ban->base.enabled = game_admin && selected_nick >= 0;
    game_btn_ban->interactive = game_admin && selected_nick >= 0;
    game_btn_ban->base.hovered =
        game_btn_ban->interactive && SDL_PointInRect(&mouse_pos, &game_btn_ban->base.rect);
    game_btn_quit->base.hovered = SDL_PointInRect(&mouse_pos, &game_btn_quit->base.rect);

    bool new_coin = false;

    if (prev_winner_declared != game_state->winner_declared) {
      coins = 0;
      coin_anim.active = false;
    }
    prev_winner_declared = game_state->winner_declared;

    if (game_state->pot > coins * game_settings->bet_amounts[0] && coins < MAX_POT_COINS) {
      int pr = g_layout.pot_radius - coin_px / 2; /* leave room for the coin's own size */
      uint32_t pb = (uint32_t)(pr > 0 ? 2 * pr : 0);
      uint32_t boundary = pb - (pb * 3 / 4) * coins / MAX_POT_COINS;
      coin_in_pot[coins].offset.x = (int)pcg32_boundedrand_r(&rng, boundary) - (int)(boundary / 2);
      coin_in_pot[coins].offset.y = (int)pcg32_boundedrand_r(&rng, boundary) - (int)(boundary / 2);
      coin_in_pot[coins].angle = pcg32_boundedrand_r(&rng, 360);
      coins++;
      new_coin = true;
    }

    /* Keep last_bettor_id current: update it whenever we can see whose coins
     * decreased (i.e. on the first frame after a new game state arrives).
     * prev_coins is stale on subsequent frames so the scan is a no-op there,
     * leaving last_bettor_id pointing at the right player. */
    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (game_state->player[i].is_connected && game_state->player[i].coins < prev_coins[i]) {
        last_bettor_id = i;
        break;
      }
    }

    if (new_coin) {
      // FIRST: compute rects
      layout_coins(coin_in_pot, &g_layout.table_center, coins);

      int last = coins - 1;

      // NOW rect is valid
      coin_anim = (CoinAnimation_t){
          .texture = coin_tex_front,
          .start = (SDL_Point){seat_pos[last_bettor_id].x, seat_pos[last_bettor_id].y},
          .end = (SDL_Point){coin_in_pot[last].rect.x, coin_in_pot[last].rect.y},
          .start_time = SDL_GetTicks(),
          .duration = 300,
          .active = true,
      };
      new_coin = false;
    }

    for (int i = 0; i < coins; i++) {
      if (i == coins - 1 && coin_anim.active) {
        continue; // last coin is currently animated
      }
      SDL_RenderCopyEx(sdl_context->renderer, coin_tex_front, NULL, &coin_in_pot[i].rect,
                       coin_in_pot[i].angle, NULL, SDL_FLIP_NONE);
    }

    render_coin_animation(sdl_context->renderer, &coin_anim);

    if (!indicator_game_name && client_state.game_choice) {
      indicator_game_name =
          create_indicator_colored(client_state.game_choice->str, font->fonts[FONT_BOLD],
                                   DC_INDICATOR_GAME_BG, DC_INDICATOR_GAME_FG);
      ui_register(&registry, &indicator_game_name->base);
      indicator_game_name->base.enabled = false; /* drawn by the dashboard */
    }

    // Update coin text before layout so widths are current
    for (int8_t id = 0; id < MAX_PLAYERS; id++) {
      if (!game_coins_tw[id])
        continue;
      if (game_state->player[id].coins != prev_coins[id]) {
        char coins_str[24] = {0};
        snprintf(coins_str, sizeof coins_str, "%" PRId32, game_state->player[id].coins);
        text_widget_set_text(game_coins_tw[id], coins_str);
        prev_coins[id] = game_state->player[id].coins;
      }
    }

    // First pass: find max column widths across all connected players
    int max_col_w[3] = {0};
    for (int8_t id = 0; id < MAX_PLAYERS; id++) {
      if (!game_nick_widgets[id] || !game_coin_widgets[id] || !game_coins_tw[id])
        continue;
      UITable_t t = {0};
      ui_table_begin(&t, 0, 0, 3);
      ui_table_add(&t, 0, 0, &game_nick_widgets[id]->base);
      ui_table_add(&t, 0, 1, &game_coin_widgets[id]->base);
      ui_table_add(&t, 0, 2, &game_coins_tw[id]->base);
      for (int c = 0; c < 3; c++)
        if (t.col_width[c] > max_col_w[c])
          max_col_w[c] = t.col_width[c];
    }
    // Ensure nick column fits the 15-character maximum nick length
    int nick_min_w = 0;
    TTF_SizeUTF8(font->fonts[FONT_BOLD], "MMMMMMMMMMMMMMM", &nick_min_w, NULL);
    if (max_col_w[0] < nick_min_w)
      max_col_w[0] = nick_min_w;

    // Second pass: layout with uniform column widths, nick left-aligned
    const int np_pad = g_layout_cfg.nameplate_pad;
    const int col_spacing_val = 20; // matches ui_table_begin default
    int total_max_w = max_col_w[0] + max_col_w[1] + max_col_w[2] + 2 * col_spacing_val;
    /* Reserved zone on the left of every nameplate for the round-opener glyph,
     * so it sits clear of the turn outline (which hugs the content). */
    const int opener_gutter = 14;
    SDL_Rect turn_outline = {0};
    SDL_Rect nameplate_rects[MAX_PLAYERS] = {0};
    for (int8_t id = 0; id < MAX_PLAYERS; id++) {
      if (!game_nick_widgets[id] || !game_coin_widgets[id] || !game_coins_tw[id])
        continue;
      UITable_t player_table = {0};
      /* Local player's nameplate centers on the window; opponents anchor to
       * their (left-origin) card row. */
      int np_origin_x = (id == my_id) ? g_layout.local_seat.x - total_max_w / 2 + opener_gutter / 2
                                      : seat_pos[id].x + g_layout_cfg.card_w / 2;
      ui_table_begin(&player_table, np_origin_x, seat_pos[id].y + (int)(g_layout_cfg.card_h * 1.2),
                     3);
      for (int c = 0; c < 3; c++)
        player_table.col_width[c] = max_col_w[c];
      player_table.col_align[0] = 1; // left-align nick
      ui_table_add(&player_table, 0, 0, &game_nick_widgets[id]->base);
      ui_table_add(&player_table, 0, 1, &game_coin_widgets[id]->base);
      ui_table_add(&player_table, 0, 2, &game_coins_tw[id]->base);
      ui_table_layout(&player_table);
      nameplate_rects[id] = (SDL_Rect){
          player_table.x - np_pad - opener_gutter, player_table.y - np_pad,
          total_max_w + np_pad * 2 + opener_gutter, player_table.row_height[0] + np_pad * 2};
      if (id == turn->id && !game_state->winner_declared) {
        const int pad = 4;
        turn_outline = (SDL_Rect){player_table.x - pad, player_table.y - pad, total_max_w + pad * 2,
                                  player_table.row_height[0] + pad * 2};
      }
    }

    for (int8_t id = 0; id < MAX_PLAYERS; id++) {
      if (nameplate_rects[id].w > 0)
        draw_nameplate(sdl_context->renderer, nameplate_rects[id], 150);
    }

    const int open_pad = g_layout_cfg.open_card_pad;
    const int open_h = coin_px / 2 + open_pad * 2;
    const int open_w = (total_max_w > 0) ? total_max_w + 2 * open_pad : 280;
    for (int8_t id = 0; id < MAX_PLAYERS; id++) {
      if (game_state->player[id].is_connected)
        continue;
      int ox = seat_pos[id].x + g_layout_cfg.card_w / 2 - open_pad;
      int oy = seat_pos[id].y + (int)(g_layout_cfg.card_h * 1.2) - open_pad;
      SDL_Rect open_rect = {ox, oy, open_w, open_h};
      draw_nameplate(sdl_context->renderer, open_rect, 70);
      if (open_seat_tw[id]) {
        ui_widget_place(&open_seat_tw[id]->base, ox + (open_w - open_seat_tw[id]->base.rect.w) / 2,
                        oy + (open_h - open_seat_tw[id]->base.rect.h) / 2);
        ui_widget_render(&open_seat_tw[id]->base);
      }
    }

    ui_render_all(&registry);

    if (turn_outline.w > 0) {
      SDL_SetRenderDrawColor(sdl_context->renderer, 255, 215, 0, 255);
      draw_rect_border(sdl_context->renderer, turn_outline);
    }

    if (game_state->round_opener_id >= 0 && game_state->round_opener_id < MAX_PLAYERS &&
        nameplate_rects[game_state->round_opener_id].w > 0) {
      SDL_Rect np = nameplate_rects[game_state->round_opener_id];
      /* Center the glyph in the gutter left of where the turn outline sits
       * (the outline's left edge is at content.x - 4 = np.x + zone_w). */
      int zone_w = np_pad + opener_gutter - 4;
      int opener_x = np.x + (zone_w - opener_tag->base.rect.w) / 2;
      int opener_y = np.y + (np.h - opener_tag->base.rect.h) / 2;
      ui_widget_place(&opener_tag->base, opener_x, opener_y);
      ui_widget_render(&opener_tag->base);
    }

    /* Game indicators: the game-name cell, plus a Deuces Wild cell when wild
     * is enabled, stacked vertically in the bottom-right corner; center-
     * justified, bordered + translucent-black like the dashboard. Cell width
     * is static (longest game name) so the stack doesn't resize. */
    /* indicator_game_name is NULL until a MSG_GAME_SELECT sets game_choice; a
     * player who joins mid-hand hasn't received one yet, so skip the stack
     * until then (otherwise inds[0] is dereferenced NULL). */
    if (indicator_game_name) {
      SDL_Renderer *r = sdl_context->renderer;
      int icp = g_layout_cfg.indicator_cell_pad;
      int dw = indicator_deuces_wild->base.rect.w;
      int cell_w = (static_gamename_w > dw ? static_gamename_w : dw) + 2 * icp;
      int cell_h = indicator_deuces_wild->base.rect.h + 2 * icp;
      int sx = g_viewport.x + g_viewport.w - g_layout_cfg.margin - cell_w;
      /* Sit just above the dashboard so the stack clears the hand / dashboard below. */
      int dash_h = g_layout_cfg.circle_timer_r * 2 + 2 * g_layout_cfg.dash_pad;
      int dash_top = g_layout.local_seat.y - g_layout_cfg.btn_hand_gap - dash_h;
      /* Deuces Wild only gets its own cell when it's enabled; for non-wild
       * games it's omitted entirely (no grayed-out placeholder). The stack is
       * bottom-anchored just above the dashboard so the game-name cell doesn't
       * float with a gap when the wild cell is absent. */
      int n_cells = client_state.deuces_wild ? 2 : 1;
      int sy = dash_top - g_layout_cfg.margin - n_cells * cell_h;
      SDL_Rect cells[2];
      Indicator_t *inds[2];
      cells[0] = (SDL_Rect){sx, sy, cell_w, cell_h};
      inds[0] = indicator_game_name;
      if (n_cells == 2) {
        cells[1] = (SDL_Rect){sx, sy + cell_h, cell_w, cell_h};
        inds[1] = indicator_deuces_wild;
      }
      for (int k = 0; k < n_cells; k++) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, DC_DASH_BG.r, DC_DASH_BG.g, DC_DASH_BG.b, DC_DASH_BG.a);
        SDL_RenderFillRect(r, &cells[k]);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        draw_3d_border(r, cells[k], 10);
      }
      for (int k = 0; k < n_cells; k++) {
        if (inds[k] == indicator_deuces_wild) {
          indicator_deuces_wild->bg_color = DC_INDICATOR_WILD_BG;
          indicator_deuces_wild->animated = true;
        }
        layout_indicator(inds[k], cells[k].x + (cells[k].w - inds[k]->base.rect.w) / 2,
                         cells[k].y + (cells[k].h - inds[k]->base.rect.h) / 2);
        ui_widget_render(&inds[k]->base);
      }
    }

    draw_3d_border(sdl_context->renderer, g_layout.msg_panel, 10);

    /* Vertical gradient: white at top → light gray at bottom */
    for (int row = 0; row < g_layout.msg_panel.h; row++) {
      float t = (float)row / (float)(g_layout.msg_panel.h - 1);
      uint8_t c = (uint8_t)(255.0f - t * 30.0f);
      SDL_SetRenderDrawColor(sdl_context->renderer, c, c, c, 255);
      SDL_RenderFillRect(
          sdl_context->renderer,
          &(SDL_Rect){g_layout.msg_panel.x, g_layout.msg_panel.y + row, g_layout.msg_panel.w, 1});
    }

    SDL_SetRenderDrawColor(sdl_context->renderer, 0, 0, 0, 255);
    draw_rect_border(sdl_context->renderer, g_layout.msg_panel);

    for (int i = 0; i < SIZEOF_STATUS_MSGS; i++)
      ui_widget_render(&status_tw[i]->base);

    /* Community card placeholders: a drop-shadow card in each undealt board slot
     * until a real card is dealt there (req 5). */
    if (community_start > 0) {
      const int bp = starting_turn->id;
      SDL_SetRenderDrawBlendMode(sdl_context->renderer, SDL_BLENDMODE_BLEND);
      for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
        if (client_state.game_choice->card_slot[card_n] != CARD_SLOT_COMMUNITY)
          continue;
        CardWidget_t *cw = &card_context[bp][card_n];
        if (!cw->is_null || cw->base.rect.w == 0)
          continue;
        SDL_Rect r = cw->base.rect;
        SDL_SetRenderDrawColor(sdl_context->renderer, 0, 0, 0, 90);
        SDL_RenderFillRect(sdl_context->renderer, &(SDL_Rect){r.x + 4, r.y + 4, r.w, r.h});
        SDL_SetRenderDrawColor(sdl_context->renderer, 0, 0, 0, 60);
        SDL_RenderFillRect(sdl_context->renderer, &r);
        SDL_SetRenderDrawColor(sdl_context->renderer, 255, 255, 255, 40);
        draw_rect_border(sdl_context->renderer, r);
      }
      SDL_SetRenderDrawBlendMode(sdl_context->renderer, SDL_BLENDMODE_NONE);
    }

    Player_t *player_ptr = starting_turn;
    for (int card_n = 0; card_n < MAX_HAND_SIZE; ++card_n) {
      do {
        // printf("%d\n", __LINE__);
        ui_widget_render(&card_context[player_ptr->id][card_n].base);

        player_ptr = get_next_connected_client(players_array, player_ptr->id);
      } while (player_ptr != starting_turn);
    }

    SDL_SetRenderDrawBlendMode(sdl_context->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdl_context->renderer, 0, 0, 0, 120);
    for (int8_t id = 0; id < MAX_PLAYERS; id++) {
      if (!game_state->player[id].is_connected || game_state->player[id].in)
        continue;
      SDL_Rect card_area = {seat_pos[id].x, seat_pos[id].y,
                            MAX_HAND_SIZE * (g_layout_cfg.card_w + g_layout_cfg.card_padding) -
                                g_layout_cfg.card_padding,
                            g_layout_cfg.card_h};
      SDL_RenderFillRect(sdl_context->renderer, &card_area);
      if (nameplate_rects[id].w > 0)
        SDL_RenderFillRect(sdl_context->renderer, &nameplate_rects[id]);
    }
    SDL_SetRenderDrawBlendMode(sdl_context->renderer, SDL_BLENDMODE_NONE);

    if (client_state.play_coin_sound) {
      ma_sound_start(
          &sound_context->coin_hit_sounds[pcg32_boundedrand_r(&rng, sound_context->coin_array_size)]
               .sound);
      client_state.play_coin_sound = false;
    }

    /* ===================== Dashboard ===================== *
     * Bottom-center framed panel above the local hand:
     *   [ bet-amount step scale ] | [ timer ] | [ action buttons ]
     * Drawn every frame. The bet_scale (widgets/step_scale.c) is laid out + drawn
     * in the left cell; the action buttons are placed into dash_act_cell and
     * rendered by the action block below (local turn only); the timer is drawn at
     * dash_timer_center later, only while a countdown is active. */
    SDL_Rect dash_act_cell = {0};
    SDL_Point dash_timer_center = {0};
    {
      const LayoutConfig_t *cfg = &g_layout_cfg;
      SDL_Renderer *r = sdl_context->renderer;
      const int pad = cfg->dash_pad;
      const int btn_h = action_bw[FOLD]->base.rect.h;
      const int timer_d = cfg->circle_timer_r * 2;

      int slider_inner = cfg->slider_w;
      /* Size the action cell to the WIDEST possible 3-button action set, so any
       * translation fits (each button already auto-sizes to its translated label). */
      int wf = action_bw[FOLD]->base.rect.w;
      int max_set = action_bw[BET]->base.rect.w + action_bw[CHECK]->base.rect.w + wf;
      int s2 = action_bw[CALL]->base.rect.w + action_bw[RAISE]->base.rect.w + wf;
      int s3 = action_bw[CALL]->base.rect.w + action_bw[COMPLETE]->base.rect.w + wf;
      int s4 = action_bw[COMPLETE]->base.rect.w + action_bw[CHECK]->base.rect.w + wf;
      if (s2 > max_set)
        max_set = s2;
      if (s3 > max_set)
        max_set = s3;
      if (s4 > max_set)
        max_set = s4;
      int act_inner = max_set + 2 * cfg->act_btn_gap;

      int slider_cell_w = slider_inner + 2 * pad;
      int timer_cell_w = timer_d + 2 * pad;
      int act_cell_w = act_inner + 2 * pad;

      int dash_h = timer_d + 2 * pad;
      int dash_y = g_layout.local_seat.y - cfg->btn_hand_gap - dash_h;

      /* timer cell centered on the window; slider to its left, actions to its right */
      /* Center the whole dashboard on the screen width (dash_x_offset nudges it). */
      int total_w = slider_cell_w + 2 * cfg->dash_divider + timer_cell_w + act_cell_w;
      int bg_x = g_center.x - total_w / 2 + cfg->dash_x_offset;
      int bg_w = total_w;
      int slider_cell_x = bg_x;
      int timer_cell_x = slider_cell_x + slider_cell_w + cfg->dash_divider;
      int act_cell_x = timer_cell_x + timer_cell_w + cfg->dash_divider;

      SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(r, DC_DASH_BG.r, DC_DASH_BG.g, DC_DASH_BG.b, DC_DASH_BG.a);
      SDL_RenderFillRect(r, &(SDL_Rect){bg_x, dash_y, bg_w, dash_h});
      SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
      draw_3d_border(r, (SDL_Rect){bg_x, dash_y, bg_w, dash_h}, cfg->timer_border);

      draw_dash_divider(
          r, (SDL_Rect){timer_cell_x - cfg->dash_divider, dash_y, cfg->dash_divider, dash_h});
      draw_dash_divider(r,
                        (SDL_Rect){timer_cell_x + timer_cell_w, dash_y, cfg->dash_divider, dash_h});

      dash_timer_center = (SDL_Point){timer_cell_x + timer_cell_w / 2, dash_y + dash_h / 2};
      dash_act_cell = (SDL_Rect){act_cell_x + pad, dash_y + (dash_h - btn_h) / 2, act_inner, btn_h};

      /* ---- bet-amount step scale (left cell) ---- */
      bet_scale->active =
          (*turn_id == my_id) && !client_state.do_discard_draw && !game_state->winner_declared;
      step_scale_set_owed(bet_scale, client_state.owed);
      step_scale_layout(
          bet_scale, (SDL_Rect){slider_cell_x + pad, dash_y + pad, slider_inner, dash_h - 2 * pad});
      ui_widget_render(&bet_scale->base);
    }

    bool my_turn = *turn_id == my_id;
    if (client_state.turn_switch || game_state->winner_declared) {
      if (!client_state.end_game_timer_set) {
        client_state.timer_start = SDL_GetTicks();
        client_state.hourglass_rotate_start = client_state.timer_start;

        if (game_state->winner_declared) {
          ma_sound_start_checked(&sound_context->sounds[SND_GAME_OVER].sound);
          client_state.end_game_timer_set = true;
        }

        // Handle timeout: If there was no action by the player, one of these
        // may be set to true
        client_state.bet_check_fold = false;
        client_state.call_raise_fold = false;
        client_state.call_complete_fold = false;
        client_state.complete_check_fold = false;
        client_state.do_discard_draw = false;

        if (my_turn && !game_state->winner_declared) {
          if (player_config->turn_notify)
            ma_sound_start_checked(&sound_context->sounds[SND_MY_TURN].sound);
        }
        client_state.last_chance_played = false;
        client_state.turn_switch = false;
      }
    }

    uint32_t now = SDL_GetTicks();
    int32_t remaining_ms = 0;
    if (game_state->winner_declared) {
      remaining_ms = (int32_t)(client_state.timer_start + game_settings->end_of_game_timeout_ms) -
                     (int32_t)now;
    } else {
      remaining_ms = (int32_t)((client_state.timer_start + game_settings->action_timeout_ms) - now);

      /* Collect the action buttons to show this frame in display order, then lay
       * them out as one row centered above the local hand (req 2). RAISE/COMPLETE
       * are only included when interactive, so the centered row has no gaps. */
      ButtonWidget_t *row[MAX_ACTIONS];
      int n_row = 0;
      bool show_amounts = false;
      bool show_discard_overlay = false;

      if (client_state.do_discard_draw) {
        for (int i = 0; i < MAX_HAND_SIZE; i++)
          if (turn->hand.card[i].face_val == DH_CARD_ACE ||
              turn->hand.card[i].face_val == DH_CARD_ACE_HIGH) {
            client_state.has_ace = true;
            break;
          }
        uint8_t max_allowed = client_state.has_ace ? 4 : 3;
        action_bw[DISCARD]->interactive = client_state.n_cards_selected <= max_allowed;
        if (!action_bw[DISCARD]->interactive) {
          show_discard_overlay = true;
          if (max_allowed != last_max_allowed) {
            char tmp[50];
            snprintf(tmp, sizeof(tmp), "You may only discard a maximum of %d cards", max_allowed);
            text_widget_set_text(discard_hint_tw, tmp);
            last_max_allowed = max_allowed;
          }
        }
        row[n_row++] = action_bw[DISCARD];
      } else if (client_state.bet_check_fold || client_state.call_raise_fold ||
                 client_state.call_complete_fold || client_state.complete_check_fold) {
        action_bw[RAISE]->interactive = game_state->raises_remaining > 0;
        action_bw[COMPLETE]->interactive = game_state->raises_remaining > 0;
        if (client_state.bet_check_fold) {
          row[n_row++] = action_bw[BET];
          row[n_row++] = action_bw[CHECK];
        } else if (client_state.call_raise_fold) {
          row[n_row++] = action_bw[CALL];
          if (action_bw[RAISE]->interactive)
            row[n_row++] = action_bw[RAISE];
        } else if (client_state.call_complete_fold) {
          row[n_row++] = action_bw[CALL];
          if (action_bw[COMPLETE]->interactive)
            row[n_row++] = action_bw[COMPLETE];
        } else { /* complete_check_fold */
          if (action_bw[COMPLETE]->interactive)
            row[n_row++] = action_bw[COMPLETE];
          row[n_row++] = action_bw[CHECK];
        }
        row[n_row++] = action_bw[FOLD];

        show_amounts = client_state.bet_check_fold || client_state.complete_check_fold ||
                       (client_state.call_raise_fold && action_bw[RAISE]->interactive) ||
                       (client_state.call_complete_fold && action_bw[COMPLETE]->interactive);
      }

      if (n_row > 0) {
        int total_w = (n_row - 1) * g_layout_cfg.act_btn_gap;
        for (int i = 0; i < n_row; i++)
          total_w += row[i]->base.rect.w;
        int bx = dash_act_cell.x + (dash_act_cell.w - total_w) / 2;
        int by = dash_act_cell.y;
        for (int i = 0; i < n_row; i++) {
          row[i]->base.rect.x = bx;
          row[i]->base.rect.y = by;
          ui_widget_render(&row[i]->base);
          bx += row[i]->base.rect.w;
          if (i < n_row - 1) {
            int dx = bx + (g_layout_cfg.act_btn_gap - g_layout_cfg.dash_btn_div) / 2;
            draw_dash_divider(sdl_context->renderer,
                              (SDL_Rect){dx, by, g_layout_cfg.dash_btn_div, dash_act_cell.h});
          }
          bx += g_layout_cfg.act_btn_gap;
        }
      }

      /* Discard-limit hint overlaid ON the local hand: translucent black box with
       * yellow text (req 3), instead of a plain line beside the buttons. */
      if (show_discard_overlay) {
        int tw_w = discard_hint_tw->base.rect.w;
        int ov_w = local_row_w > tw_w + 20 ? local_row_w : tw_w + 20;
        SDL_Rect box = {g_layout.local_seat.x - ov_w / 2, seat_pos[my_id].y, ov_w,
                        g_layout_cfg.card_h};
        SDL_SetRenderDrawBlendMode(sdl_context->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sdl_context->renderer, 0, 0, 0,
                               (Uint8)g_layout_cfg.discard_overlay_alpha);
        SDL_RenderFillRect(sdl_context->renderer, &box);
        SDL_SetRenderDrawBlendMode(sdl_context->renderer, SDL_BLENDMODE_NONE);
        ui_widget_place(&discard_hint_tw->base, box.x + (box.w - tw_w) / 2,
                        box.y + (box.h - discard_hint_tw->base.rect.h) / 2);
        ui_widget_render(&discard_hint_tw->base);
      }

      // Bet amount is chosen on the slider (drawn in the dashboard above). Here we
      // only disable amounts at/below the current bet so the slider can't pick them.
      // Disable bet amounts at/below the current bet so the scale can't pick them.
      if (show_amounts) {
        for (size_t i = 0; i < n_bet_amounts; i++)
          bet_scale->enabled[i] = !(game_state->prev_bet_amount > game_settings->bet_amounts[i] &&
                                    action_bw[RAISE]->interactive);
      }
    }

    if (remaining_ms > 0 && client_state.hourglass_rotate_start != 0) {
      uint32_t timeout_ms = game_state->winner_declared ? game_settings->end_of_game_timeout_ms
                                                        : game_settings->action_timeout_ms;

      float fill_ratio = (float)remaining_ms / (float)timeout_ms;
      if (fill_ratio > 1.0f)
        fill_ratio = 1.0f;
      if (fill_ratio < 0.0f)
        fill_ratio = 0.0f;

      if (my_turn && !game_state->winner_declared && !client_state.last_chance_played &&
          fill_ratio <= 1.0f / 3.0f) {
        if (player_config->turn_notify)
          ma_sound_start_checked(&sound_context->sounds[SND_MY_TURN_LAST_CHANCE].sound);
        client_state.last_chance_played = true;
      }

      render_circle_timer(sdl_context->renderer, dash_timer_center, fill_ratio,
                          my_turn && !game_state->winner_declared);
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%" PRIu32, game_state->pot);
    render_text_pot(buffer, g_layout.table_center, font);

    /* CTRL-H: show the best rank of your own hand, lower-right corner (#61). */
    if (g_show_hand_rank && hand_rank_tw) {
      const char *rank = local_hand_rank_name(&game_state->player[my_id].hand,
                                              client_state.game_choice, client_state.deuces_wild);
      if (rank) {
        text_widget_set_text(hand_rank_tw, _(rank));
        ui_widget_place(&hand_rank_tw->base,
                        g_viewport.x + g_viewport.w - hand_rank_tw->base.rect.w -
                            g_layout_cfg.margin,
                        g_viewport.y + g_viewport.h - hand_rank_tw->base.rect.h -
                            g_layout_cfg.margin);
        ui_widget_render(&hand_rank_tw->base);
      }
    }

    /* F1 reference overlay: drawn last so it sits on top of everything.  It is a
     * read-only panel; the loop keeps running and receiving server state beneath
     * it, so opening it mid-hand does not risk an action timeout. */
    if (show_hotkey_overlay)
      hotkey_overlay_render(sdl_context->renderer, font, true);

    SDL_RenderPresent(sdl_context->renderer);
    SDL_Delay(16);

    {
      int rmx, rmy;
      SDL_GetMouseState(&rmx, &rmy);
      float rlx, rly;
      SDL_RenderWindowToLogical(sdl_context->renderer, rmx, rmy, &rlx, &rly);
      mouse_pos.x = (int)rlx;
      mouse_pos.y = (int)rly;
    }

    /* Draw variants let you select discards even before your turn, so when the
     * discard prompt arrives you can just hit Discard (#22). The card .selected
     * flags are the source of truth; n_cards_selected is recomputed from them
     * after the poll. */
    /* Discard pre-selection is allowed only while a draw round is active (server
     * sets game_state->draw_phase), so it can't collide with betting; on your own
     * draw turn do_discard_draw is also set (see the draw_phase/do_discard_draw
     * truth table). */
    bool can_select_discards = client_state.game_choice && client_state.game_choice->n_draws > 0 &&
                               game_state->player[my_id].in && !game_state->winner_declared &&
                               game_state->draw_phase;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      /* F1 reference overlay, handled before anything else consumes the event:
       * F1 toggles it; while open, Esc closes it and every other key is
       * swallowed so an action hotkey pressed while reading does not also fire.
       * SDL_QUIT falls through so the window can always be closed. */
      if (hotkey_overlay_handle_event(&event, &show_hotkey_overlay))
        continue;

      if (can_select_discards && event.type == SDL_MOUSEBUTTONDOWN) {
        for (int card_n = 0; card_n < MAX_HAND_SIZE; card_n++) {
          if (DH_is_card_null(game_state->player[my_id].hand.card[card_n]))
            continue;
          if (SDL_PointInRect(&mouse_pos, &card_context[my_id][card_n].base.rect)) {
            card_context[my_id][card_n].base.selected =
                !card_context[my_id][card_n].base.selected;
            break;
          }
        }
      }

      /* Digit keys 1-5 toggle discard selection — but only when not choosing a
       * bet amount (the step scale uses 1-8 for that) (#22). */
      if (event.type == SDL_KEYDOWN && can_select_discards && !bet_scale->active) {
        SDL_Keycode kc = event.key.keysym.sym;
        if (kc >= SDLK_1 && kc <= SDLK_5) {
          int card_n = (int)(kc - SDLK_1);
          if (card_n < MAX_HAND_SIZE &&
              !DH_is_card_null(game_state->player[my_id].hand.card[card_n]))
            card_context[my_id][card_n].base.selected =
                !card_context[my_id][card_n].base.selected;
        }
      }
      for (int i = 0; i < MAX_ACTIONS; i++) {
        action_bw[i]->base.hovered = SDL_PointInRect(&mouse_pos, &action_bw[i]->base.rect);
      }
      if (step_scale_handle(bet_scale, &event, mouse_pos)) {
        client_state.selected_amount = step_scale_value(bet_scale);
        /* This is inside `while (SDL_PollEvent(&event))`, which is the ONLY
         * event drain per frame. A `break` here discarded every other event
         * still queued this frame — so a Call/Fold/h keypress arriving in the
         * same ~16ms frame as a bet-slider change was silently swallowed (the
         * "had to press twice" / "h works then doesn't on my turn" bugs).
         * `continue` consumes just this event and keeps draining the queue. */
        continue;
      }

      if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
          game_state->player[my_id].is_admin) {
        for (int8_t i = 0; i < MAX_PLAYERS; i++) {
          if (!game_nick_widgets[i] || !game_nick_widgets[i]->selectable)
            continue;
          if (SDL_PointInRect(&mouse_pos, &game_nick_widgets[i]->base.rect)) {
            if (selected_nick >= 0 && game_nick_widgets[selected_nick])
              game_nick_widgets[selected_nick]->base.selected = false;
            if (selected_nick == i)
              selected_nick = -1;
            else
              selected_nick = i;
            if (selected_nick >= 0)
              game_nick_widgets[selected_nick]->base.selected = true;
            break;
          }
        }
        if (selected_nick >= 0) {
          if (game_btn_kick->interactive &&
              SDL_PointInRect(&mouse_pos, &game_btn_kick->base.rect)) {
            send_kick_player(sock, selected_nick);
            selected_nick = -1;
          } else if (game_btn_ban->interactive &&
                     SDL_PointInRect(&mouse_pos, &game_btn_ban->base.rect)) {
            send_ban_player(sock, selected_nick);
            selected_nick = -1;
          }
        }
      }

      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
                 SDL_PointInRect(&mouse_pos, &game_btn_quit->base.rect) &&
                 confirm_quit(font->fonts)) {
        SDL_Event quit = {.type = SDL_QUIT};
        SDL_PushEvent(&quit);
        running = false;
      } else if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
          if (confirm_quit(font->fonts)) {
            SDL_Event quit = {.type = SDL_QUIT};
            SDL_PushEvent(&quit);
            running = false;
          }
          break;
        case SDLK_RETURN:
          if (event.key.keysym.mod & KMOD_ALT)
            toggle_fullscreen(sdl_context);
          break;

        case SDLK_F11:
          toggle_fullscreen(sdl_context);
          break;

        default:
          break;
        }

        /* Hand-rank overlay toggle: a configurable single key (#61), so it's a
         * runtime check against g_hotkey_cfg rather than a fixed switch case. */
        if (g_hotkey_cfg.hand_rank != SDLK_UNKNOWN &&
            event.key.keysym.sym == g_hotkey_cfg.hand_rank)
          g_show_hand_rank = !g_show_hand_rank;
      }
      if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN) {
        if (my_turn && !client_state.do_discard_draw) {
          if (client_state.bet_check_fold || client_state.call_raise_fold ||
              client_state.call_complete_fold || client_state.complete_check_fold) {
            if (SDL_PointInRect(&mouse_pos, &action_bw[FOLD]->base.rect) ||
                event.key.keysym.sym == action_bw[FOLD]->hotkey) {
              send_player_action(&client_state, sock, ACTION_FOLD, 0);
            }
          }
          if (client_state.bet_check_fold) {
            // TODO: use existing array (or modify it) to loop through each action
            if (SDL_PointInRect(&mouse_pos, &action_bw[BET]->base.rect) ||
                event.key.keysym.sym == action_bw[BET]->hotkey) {
              send_player_action(&client_state, sock, ACTION_BET, client_state.selected_amount);
            } else if (SDL_PointInRect(&mouse_pos, &action_bw[CHECK]->base.rect) ||
                       event.key.keysym.sym == action_bw[CHECK]->hotkey) {
              send_player_action(&client_state, sock, ACTION_CHECK, 0);
            }
          } else if (client_state.call_raise_fold) {
            if (action_bw[RAISE]->interactive &&
                (SDL_PointInRect(&mouse_pos, &action_bw[RAISE]->base.rect) ||
                 event.key.keysym.sym == action_bw[RAISE]->hotkey)) {
              send_player_action(&client_state, sock, ACTION_RAISE, client_state.selected_amount);
            } else if (SDL_PointInRect(&mouse_pos, &action_bw[CALL]->base.rect) ||
                       event.key.keysym.sym == action_bw[CALL]->hotkey) {
              send_player_action(&client_state, sock, ACTION_CALL, 0);
            }
          } else if (client_state.call_complete_fold) {
            if (action_bw[COMPLETE]->interactive &&
                (SDL_PointInRect(&mouse_pos, &action_bw[COMPLETE]->base.rect) ||
                 event.key.keysym.sym == action_bw[COMPLETE]->hotkey)) {
              send_player_action(&client_state, sock, ACTION_BET, client_state.selected_amount);
            } else if (SDL_PointInRect(&mouse_pos, &action_bw[CALL]->base.rect) ||
                       event.key.keysym.sym == action_bw[CALL]->hotkey) {
              send_player_action(&client_state, sock, ACTION_CALL, 0);
            }
          } else if (client_state.complete_check_fold) {
            if (action_bw[COMPLETE]->interactive &&
                (SDL_PointInRect(&mouse_pos, &action_bw[COMPLETE]->base.rect) ||
                 event.key.keysym.sym == action_bw[COMPLETE]->hotkey)) {
              send_player_action(&client_state, sock, ACTION_BET, client_state.selected_amount);
            } else if (SDL_PointInRect(&mouse_pos, &action_bw[CHECK]->base.rect) ||
                       event.key.keysym.sym == action_bw[CHECK]->hotkey) {
              send_player_action(&client_state, sock, ACTION_CHECK, 0);
            }
          }
        } else if (action_bw[DISCARD]->interactive && client_state.do_discard_draw &&
                   (SDL_PointInRect(&mouse_pos, &action_bw[DISCARD]->base.rect) ||
                    event.key.keysym.sym == action_bw[DISCARD]->hotkey)) {

          // Although the maximum allowed discards for 5 card draw can never
          // exceed 4, we need an array size of MAX_HAND_SIZE in case they select
          // all 5. However, the player will be required to have < 5 selected
          // to actually perform the discard.
          uint8_t discard_indices[MAX_HAND_SIZE] = {0};
          uint8_t discard_count = 0;

          for (uint8_t i = 0; i < MAX_HAND_SIZE; i++) {
            if (!card_context[my_id][i].base.selected)
              continue;
            discard_indices[discard_count++] = i;
          }

          // Reset the flag that's used to indicate to the client it's their turn to draw
          client_state.do_discard_draw = false;

          send_discards_request_new_cards(sock, discard_indices, discard_count);
          verbose_puts("Discards sent");
        }
      }
    } // End Poll event

    /* n_cards_selected derives from the card flags (single source of truth), so
     * pre-selections survive the draw prompt's counter reset and a hand rebuild
     * clears it automatically (#22). */
    client_state.n_cards_selected = 0;
    for (int k = 0; k < MAX_HAND_SIZE; k++)
      if (card_context[my_id][k].base.selected)
        client_state.n_cards_selected++;
  }
  SDL_DestroyTexture(felt_tex);
  SDL_DestroyTexture(vignette_tex);
  for (int i = 0; i < MAX_PLAYERS; i++)
    if (open_seat_tw[i])
      ui_widget_destroy(&open_seat_tw[i]->base);
  for (int i = 0; i < MAX_ACTIONS; i++)
    ui_widget_destroy(&action_bw[i]->base);
  ui_widget_destroy(&discard_hint_tw->base);
  ui_widget_destroy(&opener_tag->base);
  step_scale_destroy(bet_scale);
  for (int i = 0; i < SIZEOF_STATUS_MSGS; i++)
    ui_widget_destroy(&status_tw[i]->base);
  if (hand_rank_tw)
    ui_widget_destroy(&hand_rank_tw->base);
  ui_destroy_all(&registry);
  if (running)
    return GAME_LOGIC_AT_MENU;
  if (disconnected)
    return GAME_LOGIC_DISCONNECTED;
  return GAME_LOGIC_QUIT;
}
