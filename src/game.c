/*
 game.c
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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <SDL2/SDL.h> /* SDL_SwapBE*; replaced by portable helpers in a later step */

#include "game.h"

// If changing game type (e.g. 0x04), or adding a new game,
// the game protocol version must also be changed.
const GameChoice_t game_choices[] = {
    {FIVE_CARD_DRAW,
     "5-card draw",
     0x01,
     game_five_card_draw,
     5,
     2,
     1,
     5,
     {CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE,
      CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED},
     true},
    {FIVE_CARD_DOUBLE_DRAW,
     "5-card double draw",
     0x02,
     game_five_card_draw,
     5,
     3,
     2,
     5,
     {CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE,
      CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED},
     true},
    {FIVE_CARD_STUD,
     "5-card stud",
     0x03,
     game_stud,
     5,
     4,
     0,
     2,
     {CARD_SLOT_HOLE, CARD_SLOT_FACE_UP, CARD_SLOT_FACE_UP, CARD_SLOT_FACE_UP, CARD_SLOT_FACE_UP,
      CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED},
     true},
    {SIX_CARD_STUD,
     "6-card stud",
     0x04,
     game_stud,
     6,
     5,
     0,
     2,
     {CARD_SLOT_FACE_UP, CARD_SLOT_HOLE, CARD_SLOT_FACE_UP, CARD_SLOT_FACE_UP, CARD_SLOT_FACE_UP,
      CARD_SLOT_HOLE, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED},
     true},
    {SEVEN_CARD_STUD,
     "7-card stud",
     0x05,
     game_stud,
     7,
     5,
     0,
     3,
     {CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_FACE_UP, CARD_SLOT_FACE_UP, CARD_SLOT_FACE_UP,
      CARD_SLOT_FACE_UP, CARD_SLOT_HOLE, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED},
     true},
    {FIVE_CARD_SHOWDOWN,
     "5-card showdown",
     0x06,
     game_five_card_draw,
     5,
     1,
     0,
     5,
     {CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE,
      CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED},
     true},
    {CALIFORNIA_LOWBALL,
     "California lowball",
     0x07,
     game_five_card_draw,
     5,
     2,
     1,
     5,
     {CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE,
      CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED},
     false},
    {TEXAS_HOLDEM,
     // TRANSLATORS: Name of a poker variant. Usually left
     // untranslated.
     "Texas Hold'em",
     0x08,
     game_texas_holdem,
     7,
     4,
     0,
     2,
     {CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_COMMUNITY, CARD_SLOT_COMMUNITY, CARD_SLOT_COMMUNITY,
      CARD_SLOT_COMMUNITY, CARD_SLOT_COMMUNITY, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED},
     false},
    {OMAHA,
     // TRANSLATORS: Name of a poker variant. Usually left
     // untranslated.
     "Omaha",
     0x09,
     game_omaha,
     9,
     4,
     0,
     4,
     {CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_COMMUNITY,
      CARD_SLOT_COMMUNITY, CARD_SLOT_COMMUNITY, CARD_SLOT_COMMUNITY, CARD_SLOT_COMMUNITY},
     false},
    {SEVEN_CARD_NO_PEEK,
     "7-card no peek",
     0x0A,
     game_seven_card_no_peek,
     7,
     7,
     0,
     7,
     {CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_HOLE,
      CARD_SLOT_HOLE, CARD_SLOT_HOLE, CARD_SLOT_UNUSED, CARD_SLOT_UNUSED},
     false}};

const GameChoice_t *find_game_choice_by_type(const uint8_t type) {
  for (size_t i = 0; i < sizeof(game_choices) / sizeof(game_choices[0]); ++i) {
    if (game_choices[i].game_type == type) {
      return &game_choices[i];
    }
  }
  return NULL; // Not found
}

int send_game_select(tcpme_socket_t sock, uint8_t game_type, bool deuces_wild) {
  GameSelectPayload_t payload = {game_type, deuces_wild ? 1 : 0};

  const uint32_t payload_size = OPCODE_SIZE + sizeof(payload);
  const uint32_t total_size_be = SDL_SwapBE32(payload_size);

  uint8_t buffer[LENGTH_PREFIX_SIZE + OPCODE_SIZE + sizeof(GameSelectPayload_t)];

  // Write length prefix
  memcpy(buffer, &total_size_be, LENGTH_PREFIX_SIZE);

  // Write opcode (portable big-endian)
  uint16_t opcode_be = SDL_SwapBE16(MSG_GAME_SELECT);
  memcpy(buffer + LENGTH_PREFIX_SIZE, &opcode_be, sizeof(opcode_be));

  // Write payload
  memcpy(buffer + LENGTH_PREFIX_SIZE + OPCODE_SIZE, &payload, sizeof(payload));

  // Send all
  int result = send_all_tcp(sock, buffer, sizeof(buffer));

  const GameChoice_t *choice = find_game_choice_by_type(game_type);
  if (result == 0) {
    verbose_printf("Game type sent: %s (Deuces wild: %s)\n", choice ? choice->str : "Unknown",
                   deuces_wild ? "Yes" : "No");
    return 0;
  }

  fprintf(stderr, "Game type failed to send: %s\n", choice ? choice->str : "Unknown");
  return result;
}

int send_kick_player(tcpme_socket_t sock, int8_t target_id) {
  uint8_t payload = (uint8_t)target_id;
  return send_message(sock, MSG_KICK_PLAYER, &payload, sizeof(payload));
}

int send_ban_player(tcpme_socket_t sock, int8_t target_id) {
  uint8_t payload = (uint8_t)target_id;
  return send_message(sock, MSG_BAN_PLAYER, &payload, sizeof(payload));
}

bool get_game_select_payload(uint8_t *buffer, const uint32_t size, const int client_id,
                             GameSelectPayload_t *out) {
  if (!buffer || !out)
    return false;

  // Size check — includes opcode in this context
  if (size != OPCODE_SIZE + sizeof(GameSelectPayload_t)) {
    fprintf(stderr,
            "[NET] Invalid MSG_GAME_SELECT size from client %d "
            "(got %zu, expected %zu)\n",
            client_id, (size_t)size, (size_t)(OPCODE_SIZE + sizeof(GameSelectPayload_t)));
    return false;
  }

  memcpy(out, buffer + OPCODE_SIZE, sizeof(GameSelectPayload_t));
  return true;
}

static bool is_valid_player(const Player_t *p, bool want_all_clients) {
  return p->is_connected && (want_all_clients || p->in);
}

static Player_t *get_next_player_real(Player_t *players_array, int cur, bool want_all_clients,
                                      const char *file, const int line) {
  int i = (cur + 1) % MAX_PLAYERS;

  while (i != cur) {
    // fprintf(stderr, "i: %d\n", i);
    if (is_valid_player(&players_array[i], want_all_clients)) {
      return &players_array[i];
    }
    i = (i + 1) % MAX_PLAYERS;
  }

  // fprintf(stderr, "i: %d\n", i);
  // Final fallback: check starting index
  if (is_valid_player(&players_array[cur], want_all_clients)) {
    return &players_array[cur];
  }

  fprintf(stderr, "No valid players/clients found\n%s:%d\n", file, line);
  return NULL;
}

Player_t *get_next_player_(Player_t *players_array, int cur, const char *file, const int line) {
  const bool want_all_clients = false;
  return get_next_player_real(players_array, cur, want_all_clients, file, line);
}

Player_t *get_next_connected_client_(Player_t *players_array, int cur, const char *file,
                                     const int line) {
  const bool want_all_clients = true;
  return get_next_player_real(players_array, cur, want_all_clients, file, line);
}

void pcg_srand_auto(void) {
  uint64_t initstate = time(NULL) ^ (intptr_t)&printf;
  uint64_t initseq = (intptr_t)&pcg_srand_auto;
  pcg32_srandom_r(&rng, initstate, initseq);
}
