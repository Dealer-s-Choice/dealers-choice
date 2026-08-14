/*
 types.h
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025,2026 Andy Alt

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

#ifndef __TYPES_H
#define __TYPES_H

#include <pokeval.h>

#include "main.h"
#include "tcpme/tcpme.h"

#define MAX_PLAYERS 5
#define MAX_CLIENTS 5
#define SIZEOF_NICK 15
#define MAX_INPUT_LENGTH 64
#define MAX_BET_AMOUNTS 8

#define LEN_STATUS_STR 100

typedef enum {
  RC_ERR = -1,
  RC_OK = 0,
} EReturnCode_t;

typedef enum {
  CARD_SLOT_UNUSED,
  CARD_SLOT_HOLE,      // face down, private (draw cards, stud down-cards, hole cards)
  CARD_SLOT_FACE_UP,   // face up, private (stud up-cards)
  CARD_SLOT_COMMUNITY, // face up, shared
} CardSlotType_t;

typedef enum {
  FIVE_CARD_DRAW,
  FIVE_CARD_DOUBLE_DRAW,
  FIVE_CARD_TRIPLE_DRAW,
  FIVE_CARD_STUD,
  SIX_CARD_STUD,
  SEVEN_CARD_STUD,
  FIVE_CARD_SHOWDOWN,
  CALIFORNIA_LOWBALL,
  TEXAS_HOLDEM,
  OMAHA,
  SEVEN_CARD_NO_PEEK,
  MAX_CHOICES,
} EMenuOption_t;

typedef struct {
  char nick[SIZEOF_NICK];
  int8_t id;
  POKEVAL_Hand_9 hand;
  int32_t coins;
  bool in; // Used for spectators or when someone has folded
  bool winner;
  bool is_connected;
  bool is_admin;
  /* ed25519 public key (crypto_sign_PUBLICKEYBYTES; static_assert'd == 32 in
     dc_identity.c). The player's portable identity from the challenge-response
     handshake (#67); server-only, never serialized. */
  unsigned char identity_pk[32];
} Player_t;

typedef struct {
  uint32_t pot;
  int8_t dealer_id;
  int8_t round_opener_id; /* seat that opens the current betting round (-1 = none) */
  bool at_menu;
  uint8_t player_count;
  uint32_t raises_remaining;
  uint32_t prev_bet_amount;
  bool winner_declared;
  bool draw_phase; /* true while a draw round is active (gates discard pre-select) */
  Player_t player[MAX_PLAYERS];
} GameState_t;

typedef struct {
  int8_t client_id;
  uint32_t action_timeout_ms;
  uint32_t end_of_game_timeout_ms;
  uint32_t bet_amounts[MAX_BET_AMOUNTS];
  uint8_t bet_amount_count;
} GameSettings_t;

// A forward declaration
struct ServerConfig_t;

typedef struct {
  tcpme_socket_t *clients;
  tcpme_set_t *socket_set;
  GameState_t *game_state;
  // Server-side real (unmasked) card values — never serialized directly.
  // broadcast_game_state derives what each client may see from this array
  // and the card_slot[] configuration, ensuring hole cards are never sent.
  POKEVAL_Hand_9 real_hand[MAX_PLAYERS];
  bool *slot_taken;
  const CliArgs_t *cli_args;
  tcpme_socket_t *server_sock;
  GameSettings_t *game_settings;
  struct ServerConfig_t *config;
  uint8_t game_type;
  Player_t **starting_turn;
  int8_t turn_id;
  bool deuces_wild;
  uint8_t player_timeouts[MAX_PLAYERS];
  char ban_list[64][TCPME_ADDRSTRLEN];
  int ban_count;
  // No-peek: cards flipped face-up per player, used when building visible hands.
  int no_peek_n_flipped[MAX_PLAYERS];
  // LAN discovery responders, serviced between actions so the game stays
  // findable even while a hand is in progress. Optional: set may be NULL. The
  // IPv4 (broadcast) and IPv6 (multicast) sockets share one set; either may be
  // invalid if that family couldn't be opened.
  tcpme_socket_t lan_discovery_sock;
  tcpme_socket_t lan_discovery_sock6;
  tcpme_set_t *lan_discovery_set;
  uint16_t lan_port;
  uint32_t lan_instance_id; /* random per-process id sent in discovery replies (clients dedup on it) */
} ArgsBroadcastGameState_t;

struct GameChoice_t;
typedef void (*game_func_t)(ArgsBroadcastGameState_t *, Player_t *, DH_Deck *,
                            const struct GameChoice_t *);

typedef struct GameChoice_t {
  const EMenuOption_t g;
  const char *str;
  const uint8_t game_type;
  const game_func_t func;
  const uint8_t hand_size;
  const uint8_t n_betting_rounds, n_draws;
  const uint8_t n_cards_initial_deal;
  const CardSlotType_t card_slot[9];
  const bool deuces_wild_compatible;
} GameChoice_t;

extern const GameChoice_t game_choices[];

#endif
