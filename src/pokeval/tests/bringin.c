#include "00_test.h"

_MAIN_HEAD_

// --- POKEVAL_suit_bringin_rank ---

// clubs=0 (lowest), diamonds=1, hearts=2, spades=3 (highest)
assert(POKEVAL_suit_bringin_rank(DH_SUIT_CLUBS) == 0);
assert(POKEVAL_suit_bringin_rank(DH_SUIT_DIAMONDS) == 1);
assert(POKEVAL_suit_bringin_rank(DH_SUIT_HEARTS) == 2);
assert(POKEVAL_suit_bringin_rank(DH_SUIT_SPADES) == 3);
assert(POKEVAL_suit_bringin_rank(-1) == -1); // invalid suit

// --- POKEVAL_card_bringin_lt ---

// Lower face value brings in first (ace-high, so 2 < 3 < ... < K < A).
DH_Card two_clubs = {DH_CARD_TWO, DH_SUIT_CLUBS};
DH_Card three_clubs = {DH_CARD_THREE, DH_SUIT_CLUBS};
DH_Card ace_clubs = {DH_CARD_ACE, DH_SUIT_CLUBS};
DH_Card ace_spades = {DH_CARD_ACE, DH_SUIT_SPADES};

// 2 < 3 (face value: 2 brings in before 3)
assert(POKEVAL_card_bringin_lt(two_clubs, three_clubs) == true);
assert(POKEVAL_card_bringin_lt(three_clubs, two_clubs) == false);

// Ace is highest face value — does NOT bring in before 3
assert(POKEVAL_card_bringin_lt(ace_clubs, three_clubs) == false);
assert(POKEVAL_card_bringin_lt(three_clubs, ace_clubs) == true);

// Suit tiebreaker: same face, clubs brings in before spades
assert(POKEVAL_card_bringin_lt(ace_clubs, ace_spades) == true);
assert(POKEVAL_card_bringin_lt(ace_spades, ace_clubs) == false);

// Equal cards: not strictly less than
assert(POKEVAL_card_bringin_lt(ace_clubs, ace_clubs) == false);

// Suit ordering among equal faces: clubs < diamonds < hearts < spades
DH_Card k_clubs = {DH_CARD_KING, DH_SUIT_CLUBS};
DH_Card k_diamonds = {DH_CARD_KING, DH_SUIT_DIAMONDS};
DH_Card k_hearts = {DH_CARD_KING, DH_SUIT_HEARTS};
DH_Card k_spades = {DH_CARD_KING, DH_SUIT_SPADES};

assert(POKEVAL_card_bringin_lt(k_clubs, k_diamonds) == true);
assert(POKEVAL_card_bringin_lt(k_diamonds, k_hearts) == true);
assert(POKEVAL_card_bringin_lt(k_hearts, k_spades) == true);
assert(POKEVAL_card_bringin_lt(k_spades, k_clubs) == false);

_MAIN_TAIL_
