#include "00_test.h"

void set_hand(POKEVAL_Hand_5 *hand, int faces[POKEVAL_HAND_SIZE], int suits[POKEVAL_HAND_SIZE]) {
  for (int i = 0; i < POKEVAL_HAND_SIZE; ++i) {
    hand->card[i].face_val = (int8_t)faces[i];
    hand->card[i].suit = (int8_t)suits[i];
  }
}

void run_hand7_rank_cases(const TestCase *cases, size_t num_cases) {
  for (size_t i = 0; i < num_cases; ++i) {
    POKEVAL_Hand_5 reduced = POKEVAL_hand5_from_hand7(&cases[i].hand);
    for (int j = 0; j < POKEVAL_HAND_SIZE; j++)
      fprintf(stderr, "card: %d | ", reduced.card[j].face_val);
    fputc('\n', stderr);

    short actual_rank = POKEVAL_evaluate_hand(reduced);

    fprintf(stderr, "Test %zu: %s (Expected rank: %d, Got: %d)\n", i + 1, cases[i].description,
            cases[i].expected_rank, actual_rank);

    fputc('\n', stderr);

    assert(actual_rank == cases[i].expected_rank);

    if (cases[i].expected_cards) {
      for (int j = 0; j < POKEVAL_HAND_SIZE; j++)
        assert(reduced.card[j].face_val == cases[i].expected_cards[j]);
    }
  }
}
