#include <deckhandler.h>
#include <pokeval.h>
#include <stdio.h>
#include <string.h>

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>

#define PAD_NULL_CARDS                                                                             \
  {DH_CARD_NULL, DH_CARD_NULL}, { DH_CARD_NULL, DH_CARD_NULL }

#define _MAIN_HEAD_                                                                                \
  int main(int argc, char *argv[]) {                                                               \
    (void)argc;                                                                                    \
    (void)argv;

#define _MAIN_TAIL_                                                                                \
  return 0;                                                                                        \
  }

void set_hand(POKEVAL_Hand_5 *hand, int faces[POKEVAL_HAND_SIZE], int suits[POKEVAL_HAND_SIZE]);

/* A 7-slot hand (card[5]/card[6] may be NULL for 6-card hands), the rank it must
 * evaluate to once reduced to the best 5, and optionally the exact
 * sorted-descending faces of that best-5 (NULL = don't check). Shared by the 6-
 * and 7-card table tests, which differ only in their case data. */
typedef struct {
  POKEVAL_Hand_9 hand;
  short expected_rank;
  const int *expected_cards;
  const char *description;
} TestCase;

/* Reduce each case's hand to its best 5 (POKEVAL_hand5_from_hand7), evaluate,
 * and assert the rank — plus the exact faces when expected_cards is set. */
void run_hand7_rank_cases(const TestCase *cases, size_t num_cases);

/* Build a 5-card hand from face/suit arrays `f`/`s`, evaluate it with
 * `eval_expr` (which references the macro-local `hand`), print the rank name,
 * and assert it equals `expected_rank`. `eval_expr` lets a caller pick the plain
 * or wild evaluator — see the TEST_HAND / TEST_WILD wrappers. */
#define TEST_HAND_RANK(expected_rank, f, s, eval_expr)                                            \
  do {                                                                                            \
    int faces[POKEVAL_HAND_SIZE];                                                                 \
    int suits[POKEVAL_HAND_SIZE];                                                                 \
    static POKEVAL_Hand_5 hand;                                                                   \
    memcpy(faces, f, sizeof(faces));                                                              \
    memcpy(suits, s, sizeof(suits));                                                              \
    set_hand(&hand, faces, suits);                                                                \
    short rank = (eval_expr);                                                                     \
    fprintf(stderr, "rank: %s\n", POKEVAL_rank[rank]);                                            \
    assert(rank == expected_rank);                                                                \
  } while (0)
