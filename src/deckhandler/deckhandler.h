/**
 * @file deckhandler.h
 * @brief Library to handle a standard deck of playing cards.
 *
 * Provides functions to initialize, shuffle, and query cards in a deck.
 * Includes support for custom PCG-based randomization.
 *
 */

#ifndef __DECK_HANDLER
#define __DECK_HANDLER

#ifdef HAVE_PCG
#include <pcg_variants.h>
#else
#include <pcg_basic.h>
#endif

#include <stdbool.h>
#include <stdint.h>

/// Number of face values in a standard card deck (Ace to King).
#define NUM_OF_FACES 13

/// Enumeration of the four standard card suits.
typedef enum {
  DH_SUIT_HEARTS,   ///< Hearts suit
  DH_SUIT_DIAMONDS, ///< Diamonds suit
  DH_SUIT_SPADES,   ///< Spades suit
  DH_SUIT_CLUBS,    ///< Clubs suit
  DH_SUIT_MAX       ///< Number of suits
} DH_suit;

/// Enumeration of card face values (Ace can be high or low).
typedef enum DH_card_face {
  DH_CARD_NULL = -2,
  DH_CARD_BACK = -1,
  DH_CARD_ACE = 1, ///< Ace (low)
  DH_CARD_TWO,
  DH_CARD_THREE,
  DH_CARD_FOUR,
  DH_CARD_FIVE,
  DH_CARD_SIX,
  DH_CARD_SEVEN,
  DH_CARD_EIGHT,
  DH_CARD_NINE,
  DH_CARD_TEN,
  DH_CARD_JACK,
  DH_CARD_QUEEN,
  DH_CARD_KING,
  DH_CARD_ACE_HIGH ///< Ace (high) for straight evaluation, not dealt
} DH_card_face;

/// Total number of cards in a standard deck.
#define DH_CARDS_IN_DECK 52

/**
 * @DH_Card
 * @brief Represents a single playing card with a face value and suit.
 */
typedef struct {
  int32_t face_val; ///< Value of the card face (1–13)
  int32_t suit;     ///< Suit of the card (see enum)
} DH_Card;

extern const DH_Card DH_card_back;
extern const DH_Card DH_card_null;

bool DH_is_card_back(DH_Card a);

bool DH_is_card_null(DH_Card a);

/**
 * @DH_Deck
 * @brief Represents a full deck of 52 playing cards.
 */
typedef struct {
  DH_Card card[DH_CARDS_IN_DECK];    ///< The deck: always holds all 52 cards; dealing only advances top_card
  DH_Card discard[DH_CARDS_IN_DECK]; ///< Muck: copies of discarded cards, dealt once the deck is exhausted
  int top_card;                      ///< Next card to deal from the deck
  int n_discard;                     ///< Live cards in the muck (front of discard[]); recycled FIFO
  int discard_shuffled;              ///< How many leading muck cards are already shuffled into order
} DH_Deck;

/**
 * @brief Seed the PCG random number generator with user-supplied values.
 *
 * @param initstate Initialization state value.
 * @param initseq   Initialization sequence value.
 */
void DH_pcg_srand(uint64_t initstate, uint64_t initseq);

/**
 * @brief Automatically seed the PCG random number generator with internal defaults.
 *
 * Uses `time(NULL)` and pointer values for entropy.
 */
void DH_pcg_srand_auto(void);

/**
 * @brief Create and initialize a new deck of cards.
 *
 * This function returns a new `dh_deck` struct that is initialized to a full, shuffled deck.
 * Internally, it calls `DH_init_deck()` to perform the initialization.
 *
 * @return A fully initialized deck of cards.
 */
DH_Deck DH_get_new_deck(void);

/**
 * @brief Deal the top card from the deck.
 *
 * Deals the next card from the deck and advances the deck position. When the
 * deck is dealt out, the discard pile (see DH_discard_card) is shuffled and
 * dealt from. If the deck is exhausted and the muck is empty, `DH_card_null`
 * is returned (it never silently re-deals a card already in play).
 *
 * @param deck Pointer to the deck from which to deal a card.
 * @return The dealt card, or `DH_card_null` if no cards remain.
 */
DH_Card DH_deal_top_card(DH_Deck *deck);

/**
 * @brief Place a card onto the deck's discard pile (muck).
 *
 * Discarded cards are recycled into a fresh draw pile, shuffled, once the draw
 * pile is exhausted. The caller owns the policy of what to discard (e.g. cards
 * a player replaced on a draw); the deck never moves cards to the muck on its
 * own.
 *
 * @param deck Pointer to the deck.
 * @param card The card to discard.
 */
void DH_discard_card(DH_Deck *deck, DH_Card card);

/**
 * @brief Number of cards still dealable: the draw pile plus the discard pile.
 *
 * @param deck Pointer to the deck.
 * @return Dealable card count.
 */
int DH_cards_remaining(const DH_Deck *deck);

/**
 * @brief Fisher-Yates shuffle of the first `n` cards of an array.
 *
 * Used to shuffle a recycled discard pile; kept separate from DH_shuffle_deck
 * so the full-deck shuffle's seeded output is never disturbed.
 *
 * @param cards Pointer to the card array.
 * @param n     Number of leading cards to shuffle.
 */
void DH_shuffle_partial(DH_Card *cards, int n);

/**
 * @brief Shuffle the full deck using the PCG random number generator.
 *
 * Shuffles all 52 cards and resets the deal position. Any cards in the discard
 * pile (muck) are collected back into the deck, so calling this at the start of
 * each hand restores a complete, freshly shuffled deck without re-initializing.
 *
 * @param deck_dh Pointer to the deck to shuffle.
 */
void DH_shuffle_deck(DH_Deck *deck_dh);

/**
 * @brief Cuts the deck at the specified index, simulating a real-life card cut.
 *
 * This function rotates the deck so that the card at `cut_point` becomes the new top card.
 * The cards before the cut point are moved to the bottom of the deck. For example, if the
 * deck is cut at position 26, the new order will be cards 26 to 51 followed by cards 0 to 25.
 *
 * @param deck Pointer to the DH_Deck structure to be modified.
 * @param cut_point The index at which to cut the deck. Must be between 1 and DH_CARDS_IN_DECK - 1.
 *                  If out of bounds, the function does nothing.
 *
 * @note This function modifies the deck in-place. Use in conjunction with DH_shuffle_deck()
 *       to simulate realistic shuffling behavior.
 *
 * @see DH_shuffle_deck
 */
void DH_cut_deck(DH_Deck *deck, int cut_point);

/**
 * @brief Get the string name of a card's face value (e.g., "Ace", "10", "King").
 *
 * @param card The card to query.
 * @return Pointer to a constant string.
 */
const char *DH_get_card_face(DH_Card card);

/**
 * @brief Get the string name of a card's suit (e.g., "Hearts", "Spades").
 *
 * @param card The card to query.
 * @return Pointer to a constant string.
 */
const char *DH_get_card_suit(DH_Card card);

/**
 * @brief Get the Unicode symbol representing the card's suit.
 *
 * ♥ ♠ ♦ ♣ depending on the suit.
 *
 * @param card The card to query.
 * @return Pointer to a UTF-8 encoded Unicode string.
 */
const char *DH_get_card_unicode_suit(DH_Card card);

const char *DH_get_unicode_suit(DH_suit suit);

/**
 * @brief Get the string name of a face value given its integer representation.
 *
 * @param val Integer value representing a face (1–13).
 * @return Pointer to a constant string.
 */
const char *DH_get_card_face_str(int val);

#endif // __DECK_HANDLER
