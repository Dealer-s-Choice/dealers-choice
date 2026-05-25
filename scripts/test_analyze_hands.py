#!/usr/bin/env python3
"""Sanity tests for analyze_hands.py's poker evaluator.

These exercise the cases the analyzer needs to get right to be useful as a
cross-check on the server: standard high-hand ranks, the A-5 wheel,
California lowball ordering, deuces-wild upgrades (including five-of-a-kind),
and Omaha's 2-hole-3-community constraint.

Run: python3 scripts/test_analyze_hands.py
Exit code is non-zero if any assertion fails.
"""

from __future__ import annotations

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import analyze_hands as ah  # noqa: E402


def C(face: int, suit: int = 0, slot: int = 1) -> dict:
    return {"f": face, "s": suit, "slot": slot}


def assert_eq(lhs, rhs, msg=""):
    if lhs != rhs:
        raise AssertionError(f"{msg}: {lhs!r} != {rhs!r}")


def assert_gt(lhs, rhs, msg=""):
    if not (lhs > rhs):
        raise AssertionError(f"{msg}: expected {lhs!r} > {rhs!r}")


def test_high_card_vs_pair():
    high = ah.best5([C(2, 0), C(5, 1), C(7, 2), C(9, 3), C(13, 0)], lowball=False, wild_face=None)
    pair = ah.best5([C(2, 0), C(2, 1), C(7, 2), C(9, 3), C(13, 0)], lowball=False, wild_face=None)
    assert_gt(pair, high, "pair > high card")


def test_two_pair_vs_pair():
    two_pair = ah.best5([C(5, 0), C(5, 1), C(8, 2), C(8, 3), C(13, 0)],
                        lowball=False, wild_face=None)
    pair = ah.best5([C(5, 0), C(5, 1), C(7, 2), C(9, 3), C(13, 0)],
                    lowball=False, wild_face=None)
    assert_gt(two_pair, pair, "two pair > pair")


def test_straight_high_ace():
    straight = ah.best5([C(10, 0), C(11, 1), C(12, 2), C(13, 3), C(1, 0)],
                        lowball=False, wild_face=None)
    flush = ah.best5([C(2, 1), C(5, 1), C(7, 1), C(9, 1), C(13, 1)],
                     lowball=False, wild_face=None)
    assert_eq(straight[0], ah.STRAIGHT, "broadway is straight")
    assert_gt(flush, straight, "flush > straight")


def test_wheel_straight():
    wheel = ah.best5([C(1, 0), C(2, 1), C(3, 2), C(4, 3), C(5, 0)],
                     lowball=False, wild_face=None)
    assert_eq(wheel[0], ah.STRAIGHT, "wheel is a straight in high")
    assert_eq(wheel[1], 5, "wheel high card is 5")


def test_straight_flush_beats_quads():
    sf = ah.best5([C(5, 1), C(6, 1), C(7, 1), C(8, 1), C(9, 1)],
                  lowball=False, wild_face=None)
    quads = ah.best5([C(13, 0), C(13, 1), C(13, 2), C(13, 3), C(2, 0)],
                     lowball=False, wild_face=None)
    assert_gt(sf, quads, "straight flush > quads")


def test_royal_flush():
    royal = ah.best5([C(10, 1), C(11, 1), C(12, 1), C(13, 1), C(1, 1)],
                     lowball=False, wild_face=None)
    assert_eq(royal[0], ah.ROYAL_FLUSH, "royal flush detected")


def test_best5_of_7_holdem():
    hole = [C(1, 0, 1), C(1, 1, 1)]
    board = [C(1, 2, 3), C(13, 3, 3), C(5, 0, 3), C(7, 1, 3), C(2, 2, 3)]
    rank = ah.best5(hole + board, lowball=False, wild_face=None)
    assert_eq(rank[0], ah.THREE, "trip aces from board")


def test_deuces_wild_makes_five_of_a_kind():
    cards = [C(13, 0), C(13, 1), C(13, 2), C(2, 3), C(2, 0)]
    rank = ah.best5(cards, lowball=False, wild_face=2)
    assert_eq(rank[0], ah.FIVE_OF_A_KIND, "two wilds + trip kings = 5oaK")


def test_deuces_wild_upgrades_pair_to_trips():
    cards = [C(7, 0), C(7, 1), C(2, 2), C(11, 3), C(13, 0)]
    rank = ah.best5(cards, lowball=False, wild_face=2)
    assert_eq(rank[0], ah.THREE, "pair + one wild = three of a kind")


def test_lowball_returns_max_dup_first():
    # The wheel has max-duplicate-count of 1 (all distinct), so the first
    # tuple element is -1.  This matches pokeval's classification.
    cards = [C(1, 0), C(2, 1), C(3, 2), C(4, 3), C(5, 0)]
    rank = ah.best5(cards, lowball=True, wild_face=None)
    assert rank[0] == -1, f"unpaired hands have max_dup=1: {rank}"


def test_lowball_paired_hands_high_down():
    # pair-of-7s (7,7,6,4,3) beats pair-of-6s (Q,J,T,6,6) in A-5 because
    # both have max_dup=2 and the high-down compare runs 7 vs Q first.
    pair_7s = ah.best5([C(7, 0), C(7, 1), C(6, 2), C(4, 3), C(3, 0)],
                       lowball=True, wild_face=None)
    pair_6s = ah.best5([C(11, 0), C(6, 1), C(12, 2), C(10, 3), C(6, 0)],
                       lowball=True, wild_face=None)
    assert_gt(pair_7s, pair_6s, "pair-7s (low kickers) beats pair-6s (Q-high kickers) in A-5")


def test_lowball_wheel_beats_six_high():
    wheel = ah.best5([C(1, 0), C(2, 1), C(3, 2), C(4, 3), C(5, 0)],
                     lowball=True, wild_face=None)
    six_high = ah.best5([C(1, 0), C(2, 1), C(3, 2), C(4, 3), C(6, 0)],
                        lowball=True, wild_face=None)
    assert_gt(wheel, six_high, "wheel A-2-3-4-5 beats 6-high in A-5 lowball")


def test_lowball_pair_loses_to_high_card():
    pair = ah.best5([C(5, 0), C(5, 1), C(7, 2), C(9, 3), C(13, 0)],
                    lowball=True, wild_face=None)
    high = ah.best5([C(1, 0), C(3, 1), C(5, 2), C(7, 3), C(9, 0)],
                    lowball=True, wild_face=None)
    assert_gt(high, pair, "any unpaired hand beats any pair in lowball")


def test_omaha_must_use_exactly_two_hole():
    # Player has 4 hole cards that on their own would be quad aces, but
    # in Omaha they can use only 2 — so the best hand is two-pair
    # (aces + board pair), not quads.
    hole = [C(1, 0, 1), C(1, 1, 1), C(1, 2, 1), C(1, 3, 1)]
    board = [C(5, 0, 3), C(5, 1, 3), C(8, 2, 3), C(11, 3, 3), C(13, 0, 3)]
    rank = ah.best5_omaha(hole, board, wild_face=None)
    assert rank[0] == ah.TWO_PAIR, f"omaha must use exactly 2 hole: {rank}"


def main() -> int:
    tests = [v for k, v in globals().items() if k.startswith("test_") and callable(v)]
    failed = 0
    for t in tests:
        try:
            t()
            print(f"ok   {t.__name__}")
        except AssertionError as e:
            failed += 1
            print(f"FAIL {t.__name__}: {e}")
    print(f"\n{len(tests) - failed}/{len(tests)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
