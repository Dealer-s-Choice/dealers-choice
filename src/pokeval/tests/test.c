#include "00_test.h"

#define TEST_HAND(expected_rank, f, s)                                                             \
  do {                                                                                             \
    int faces[POKEVAL_HAND_SIZE];                                                                  \
    int suits[POKEVAL_HAND_SIZE];                                                                  \
    static POKEVAL_Hand_5 hand;                                                                    \
    memcpy(faces, f, sizeof(faces));                                                               \
    memcpy(suits, s, sizeof(suits));                                                               \
    set_hand(&hand, faces, suits);                                                                 \
    short rank = POKEVAL_evaluate_hand(hand);                                                      \
    fprintf(stderr, "rank: %s\n", POKEVAL_rank[rank]);                                             \
    assert(rank == expected_rank);                                                                 \
  } while (0)

static void test_static_hands(void) {
  int rf_faces[] = {DH_CARD_KING, DH_CARD_QUEEN, DH_CARD_ACE, DH_CARD_JACK, DH_CARD_TEN};
  int rf_suits[] = {DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS};
  TEST_HAND(POKEVAL_ROYAL_FLUSH, rf_faces, rf_suits);

  int fh_faces[] = {DH_CARD_ACE, DH_CARD_ACE, DH_CARD_ACE, DH_CARD_JACK, DH_CARD_JACK};
  int fh_suits[] = {DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_DIAMONDS, DH_SUIT_SPADES,
                    DH_SUIT_HEARTS};
  TEST_HAND(POKEVAL_FULL_HOUSE, fh_faces, fh_suits);

  int fl_faces[] = {DH_CARD_ACE, DH_CARD_THREE, DH_CARD_EIGHT, DH_CARD_JACK, DH_CARD_QUEEN};
  int fl_suits[] = {DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_HEARTS};
  TEST_HAND(POKEVAL_FLUSH, fl_faces, fl_suits);

  int st_faces[] = {DH_CARD_TWO, DH_CARD_FOUR, DH_CARD_THREE, DH_CARD_ACE, DH_CARD_FIVE};
  int st_suits[] = {DH_SUIT_HEARTS, DH_SUIT_HEARTS, DH_SUIT_CLUBS, DH_SUIT_HEARTS, DH_SUIT_SPADES};
  TEST_HAND(POKEVAL_STRAIGHT, st_faces, st_suits);
}

_MAIN_HEAD_

int cases[] = {POKEVAL_THREE_OF_A_KIND, POKEVAL_HIGH_CARD, POKEVAL_PAIR,
               POKEVAL_HIGH_CARD,       POKEVAL_PAIR,      POKEVAL_PAIR,
               POKEVAL_TWO_PAIR,        POKEVAL_PAIR};

DH_Deck deck = DH_get_new_deck();
DH_pcg_srand(1, 1);

int i;
for (size_t t = 0; t < sizeof cases / sizeof cases[0]; t++) {
  i = 0;
  DH_shuffle_deck(&deck);

  POKEVAL_Hand_5 hand;
  int k = 0;
  do {
    hand.card[k].suit = deck.card[i].suit;
    hand.card[k].face_val = deck.card[i].face_val;

    i++;
  } while (++k < POKEVAL_HAND_SIZE);

  short rank = POKEVAL_evaluate_hand(hand);
  fprintf(stderr, "rank: %s\n", POKEVAL_rank[rank]);
  assert(rank == cases[t]);
}

test_static_hands();

_MAIN_TAIL_
