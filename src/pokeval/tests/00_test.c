#include "00_test.h"

void set_hand(POKEVAL_Hand_5 *hand, int faces[POKEVAL_HAND_SIZE], int suits[POKEVAL_HAND_SIZE]) {
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
    hand->card[i].face_val = (int8_t)faces[i];
    hand->card[i].suit = (int8_t)suits[i];
  }
}
