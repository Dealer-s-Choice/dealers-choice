#!/usr/bin/env python3
"""Independently rank the showdown hands logged by --server-log-hands and
verify the server's declared winner.

Reads the JSON-lines file written by the server (one line per showdown or
fold-out win) and, for each non-fold entry, computes the best 5-card hand
for every player using its own poker evaluator, then asserts that the set
of players the server declared winners matches the set this evaluator
would pick.

Supports:
  - 5-card high (draw, showdown, stud, double draw, 5/6/7 card stud)
  - California lowball (A-5, straights/flushes don't count, wheel = best)
  - Texas Hold'em (best 5 of 7)
  - Omaha (best 5 from exactly 2 hole + 3 community)
  - Deuces wild (every 2 is wild, in any of the above)
  - 7-card no-peek (high)

Usage:
  scripts/analyze_hands.py path/to/hands.jsonl
Exit code is non-zero if any mismatch is found.
"""

from __future__ import annotations

import json
import sys
from itertools import combinations
from typing import Iterable

# Face values match DH_card_face: A=1..K=13, ace-high=14 used internally.
SUITS = {0: "h", 1: "d", 2: "s", 3: "c"}
FACES_HIGH = {1: "A", 2: "2", 3: "3", 4: "4", 5: "5", 6: "6", 7: "7", 8: "8",
              9: "9", 10: "T", 11: "J", 12: "Q", 13: "K", 14: "A"}

# Categories — higher is better. Matches POKEVAL ordering (HIGH=0 .. FIVE=10).
HIGH_CARD = 0
PAIR = 1
TWO_PAIR = 2
THREE = 3
STRAIGHT = 4
FLUSH = 5
FULL_HOUSE = 6
FOUR = 7
STRAIGHT_FLUSH = 8
ROYAL_FLUSH = 9
FIVE_OF_A_KIND = 10

CAT_NAMES = {
    HIGH_CARD: "high card", PAIR: "pair", TWO_PAIR: "two pair", THREE: "three of a kind",
    STRAIGHT: "straight", FLUSH: "flush", FULL_HOUSE: "full house", FOUR: "four of a kind",
    STRAIGHT_FLUSH: "straight flush", ROYAL_FLUSH: "royal flush", FIVE_OF_A_KIND: "five of a kind",
}


def card_str(c: dict) -> str:
    return f"{FACES_HIGH.get(c['f'], '?')}{SUITS.get(c['s'], '?')}"


def hand_str(cards: Iterable[dict]) -> str:
    return " ".join(card_str(c) for c in cards)


def _eval5_high(faces: list[int], suits: list[int]) -> tuple:
    """Rank a concrete 5-card hand for high-hand games (no wilds).

    Returns a tuple comparable by Python's default ordering — higher tuple
    means better hand.  Aces are counted high for kickers, but also low for
    the A-2-3-4-5 wheel.
    """
    counts: dict[int, int] = {}
    for f in faces:
        # Treat ace as 14 for kickers/straights here.
        v = 14 if f == 1 else f
        counts[v] = counts.get(v, 0) + 1
    # Sort by (count, value) desc — kicker order
    grouped = sorted(counts.items(), key=lambda kv: (kv[1], kv[0]), reverse=True)
    counts_sorted = [c for _, c in grouped]
    values_sorted = [v for v, _ in grouped]

    is_flush = len(set(suits)) == 1
    vs = sorted({(14 if f == 1 else f) for f in faces}, reverse=True)
    is_straight = False
    straight_high = 0
    if len(vs) == 5:
        if vs[0] - vs[4] == 4:
            is_straight = True
            straight_high = vs[0]
        elif vs == [14, 5, 4, 3, 2]:
            # Wheel A-2-3-4-5
            is_straight = True
            straight_high = 5

    if is_straight and is_flush:
        if straight_high == 14:
            return (ROYAL_FLUSH,)
        return (STRAIGHT_FLUSH, straight_high)
    if counts_sorted[0] == 4:
        return (FOUR, values_sorted[0], values_sorted[1])
    if counts_sorted[:2] == [3, 2]:
        return (FULL_HOUSE, values_sorted[0], values_sorted[1])
    if is_flush:
        return (FLUSH, *sorted((14 if f == 1 else f for f in faces), reverse=True))
    if is_straight:
        return (STRAIGHT, straight_high)
    if counts_sorted[0] == 3:
        kickers = sorted((v for v in values_sorted[1:]), reverse=True)
        return (THREE, values_sorted[0], *kickers)
    if counts_sorted[:2] == [2, 2]:
        pairs = sorted(values_sorted[:2], reverse=True)
        return (TWO_PAIR, *pairs, values_sorted[2])
    if counts_sorted[0] == 2:
        kickers = sorted(values_sorted[1:], reverse=True)
        return (PAIR, values_sorted[0], *kickers)
    return (HIGH_CARD, *sorted((14 if f == 1 else f for f in faces), reverse=True))


def _eval5_lowball(faces: list[int]) -> tuple:
    """California A-5 lowball ranking — straights and flushes don't count.

    Matches pokeval's compare_lowball_5: classify by the two largest
    duplicate counts — (4,1) quads, (3,2) full house, (3,1) trips,
    (2,2) two pair, (2,1) one pair, (1,1) no pair, lower class wins —
    then compare grouped card values (count desc, value desc, the inverse
    of the high-hand ranking within a class), lowest first-differing value
    wins.  Aces play low.

    Lower-is-better is converted to higher-is-better by negating each
    numeric component, so the caller can keep the 'bigger tuple wins'
    invariant.
    """
    vals = [1 if f == 1 else f for f in faces]
    counts: dict[int, int] = {}
    for v in vals:
        counts[v] = counts.get(v, 0) + 1
    dups = sorted(counts.values(), reverse=True)
    max_dup = dups[0]
    second_dup = dups[1] if len(dups) > 1 else 0
    key = []
    for v, c in sorted(counts.items(), key=lambda kv: (-kv[1], -kv[0])):
        key.extend([v] * c)
    # (max_neg, second_neg, grouped_vals_neg...) — bigger tuple wins.
    return (-max_dup, -second_dup, *(-x for x in key))


def best5(cards: list[dict], lowball: bool, wild_face: int | None) -> tuple:
    """Choose the best 5-card combination from up to 9 dealt cards."""
    if len(cards) < 5:
        # Pad with imaginary low cards so a partial hand still ranks lowest.
        return (HIGH_CARD,)
    best: tuple | None = None
    for combo in combinations(cards, 5):
        rank = _rank_5_with_wilds(combo, lowball, wild_face)
        if best is None or rank > best:
            best = rank
    assert best is not None
    return best


def _rank_5_with_wilds(combo: tuple[dict, ...], lowball: bool, wild_face: int | None) -> tuple:
    """Rank a 5-card hand, expanding wilds to the best concrete substitution.

    Pokeval allows the wild to play as its own face (e.g. a 2 in deuces wild
    can still be a 2 — important for the A-2-3-4-5 wheel where the 2 is the
    wild itself).  We model that by also considering the "treat the wild as
    literal" choice once per wild card.  Implementation: tag each wild whose
    contribution is finalized with `"locked": True` so the recursion stops
    iterating on it but the downstream evaluator still sees its face_val."""
    has_unlocked_wild = (
        wild_face is not None
        and any(c["f"] == wild_face and not c.get("locked") for c in combo)
    )
    if not has_unlocked_wild:
        faces = [c["f"] for c in combo]
        suits = [c["s"] for c in combo]
        if lowball:
            return _eval5_lowball(faces)
        # Five-of-a-kind has no representation in the high evaluator (it's
        # impossible with a 52-card deck) so detect it here when wilds have
        # filled every card with the same face.
        if len(set(faces)) == 1:
            return (FIVE_OF_A_KIND, faces[0] if faces[0] != 1 else 14)
        return _eval5_high(faces, suits)
    wild_idx = next(
        i for i, c in enumerate(combo) if c["f"] == wild_face and not c.get("locked")
    )
    best: tuple | None = None
    for face in range(1, 14):
        for suit in range(4):
            sub = list(combo)
            if face == wild_face:
                # Wild plays as itself — pin the original suit and lock so the
                # recursion doesn't keep substituting this card forever.
                sub[wild_idx] = {"f": face, "s": combo[wild_idx]["s"], "locked": True}
            else:
                sub[wild_idx] = {"f": face, "s": suit}
            rank = _rank_5_with_wilds(tuple(sub), lowball, wild_face)
            if best is None or rank > best:
                best = rank
            if face == wild_face:
                break  # suit was forced; don't iterate the others
    assert best is not None
    return best


def best5_omaha(hole: list[dict], community: list[dict], wild_face: int | None) -> tuple:
    """Omaha: exactly 2 hole + 3 community.  Always high-hand here."""
    best: tuple | None = None
    for h2 in combinations(hole, 2):
        for c3 in combinations(community, 3):
            rank = _rank_5_with_wilds(h2 + c3, lowball=False, wild_face=wild_face)
            if best is None or rank > best:
                best = rank
    assert best is not None
    return best


def evaluate_player(player: dict, game_type: int, deuces_wild: bool) -> tuple:
    """Pick the best 5-card hand for a player given the game variant."""
    cards = player["cards"]
    lowball = (game_type == 0x07)  # CALIFORNIA_LOWBALL
    wild_face = 2 if deuces_wild else None
    if game_type == 0x09:  # OMAHA
        hole = [c for c in cards if c["slot"] == 1]
        community = [c for c in cards if c["slot"] == 3]
        return best5_omaha(hole, community, wild_face)
    return best5(cards, lowball, wild_face)


def analyze(path: str) -> int:
    mismatches = 0
    showdowns = 0
    folds = 0
    with open(path) as f:
        for line_no, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            entry = json.loads(line)
            if entry.get("by_fold"):
                folds += 1
                continue
            showdowns += 1
            game_type = entry["game_type"]
            deuces = entry.get("deuces_wild", False)
            scored = []
            for p in entry["players"]:
                rank = evaluate_player(p, game_type, deuces)
                scored.append((rank, p))
            best_rank = max(rank for rank, _ in scored)
            our_winner_ids = {p["id"] for rank, p in scored if rank == best_rank}
            server_winner_ids = {p["id"] for p in entry["players"] if p["won"]}
            if our_winner_ids != server_winner_ids:
                mismatches += 1
                print(
                    f"MISMATCH line={line_no} game={entry['game']!r} "
                    f"deuces_wild={deuces} pot={entry['pot']}"
                )
                for rank, p in scored:
                    print(
                        f"  id={p['id']} nick={p['nick']:<8} cards=[{hand_str(p['cards'])}] "
                        f"server_won={p['won']} analyzer_rank={CAT_NAMES.get(rank[0], rank[0])} "
                        f"raw={rank}"
                    )
                print(
                    f"  server winners: {sorted(server_winner_ids)} | "
                    f"analyzer winners: {sorted(our_winner_ids)}"
                )

    print(
        f"\nAnalyzed {showdowns} showdown(s) and {folds} fold(s); "
        f"{mismatches} mismatch(es)."
    )
    return 1 if mismatches else 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} path/to/hands.jsonl", file=sys.stderr)
        sys.exit(2)
    sys.exit(analyze(sys.argv[1]))
