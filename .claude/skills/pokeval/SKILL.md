---
name: pokeval
description: How to work on Dealer's Choice's `pokeval` poker hand evaluator (src/pokeval/) — its card/ACE numbering (and the coupling to deckhandler), the full evaluation surface (5-card, 7→best-5, Omaha, wild, lowball, stud bring-in/upcard), the destructive-sort ACE-mutation trap, the wild-card tie-break dispatch that has been the recurring source of bugs, and the fuzz harness that must gate any change. Use when editing or reviewing anything in `src/pokeval/`, changing hand ranking / tie-break / wild-card / lowball logic, touching how sorted hands are broadcast, or debugging a "wrong winner" / wrong-hand-rank report.
---

# pokeval — DC's poker hand evaluator

`src/pokeval/` evaluates and compares poker hands (`pokeval.c` ~1470 lines,
`pokeval.h` ~117, a broad unit suite + a fuzz harness under `src/pokeval/tests/`).
It's linked into `libdc_core`, so the server, bot, and (via debug) client all use
it. It was moved **in-tree** at 0.0.14 and its upstream is archived — edit and
commit it here directly, it is not a subproject.

This is subtle, bug-prone code: many overlapping evaluation modes and a lot of
tie-break math. Prefer adding a test over trusting a change by eye.

## Card / ACE numbering — the thing that trips everyone

pokeval builds on `deckhandler.h`. Cards are `DH_Card { int32_t face_val, suit; }`.

- **Faces are dealt as `DH_CARD_ACE = 1` … `DH_CARD_KING = 13`.** An ace on the
  wire / in a deck is **1**, not 14.
- **`POKEVAL_ACE` is `DH_CARD_KING + 1 = 14`** — an *evaluator-internal* high-ace
  value, so an ace sorts above a king for straight/high detection. It is **not** a
  value any dealt card should carry outside the evaluator.
- `face_rank(val)` (`pokeval.c:46`) maps `DH_CARD_ACE→14` **without mutating** —
  use this pattern when you need ace-high ordering for a comparison. Contrast with
  `POKEVAL_sort_hand_for_eval`, which mutates (see below).
- **Suit ordering is context-dependent, a real footgun.** deckhandler's enum is
  hearts=0, diamonds=1, spades=2, clubs=3. pokeval's *poker* suit rank for stud
  bring-in is the opposite convention — clubs=0 (lowest, forced to bring in) …
  spades=3 (`POKEVAL_suit_bringin_rank`). Don't assume one ordering is "the" suit
  order; check which you're in.

## The destructive-sort trap (read before touching sort or broadcast)

Verified against the code 2026-08-29; #360 reshaped this and the old advice was
inverted in one place, so do not trust memory here.

- **`POKEVAL_sort_hand_for_eval`** sorts descending **and rewrites every
  `DH_CARD_ACE`(1) to `POKEVAL_ACE`(14) in place**. This is the destructive one.
  Deliberate — straight/high logic wants aces at 14 — but the hand afterwards
  carries a face value that is not legal on the wire or on screen. Safe only on a
  scratch copy.
- **`POKEVAL_sort_hand_display`** is the same sort with the ace raise undone
  before it returns. Anything that broadcasts, logs or renders wants this one.
- **`POKEVAL_sort_hand_lowball` is NOT destructive** — despite what #360's text
  claims. It orders via `lowball_value()`, a *read*, and swaps whole cards; it
  never writes `face_val`. Checked directly: no assignment to `face_val` anywhere
  in its body. Do not "fix" it.
- **The best-5-of-N selectors return display-safe hands.**
  `POKEVAL_hand5_from_hand7`, `..._wild` and `POKEVAL_hand5_omaha` sort candidates
  for evaluation internally but restore aces before returning
  (`restore_display_aces`), so a caller may broadcast or render the result
  directly. The evaluators re-raise aces themselves, so feeding a returned hand
  back into `POKEVAL_evaluate_hand()` is still correct.

The old ACE-leak bug was a sort-then-broadcast path shipping `face_val = 14` to
non-GUI clients. `handle_sort_hand` (**`src/server/round.c`**, not server.c) was
the manual mitigation; it now just calls the display sort, and the restore lives
inside pokeval where every caller gets it.

**The rule that survives all of this:** `POKEVAL_ACE` is evaluator-internal. If a
hand can leave the evaluator — wire, JSON log, screen — its aces must read
`DH_CARD_ACE`. When adding a new sort or selector, put the restore in pokeval
rather than asking each caller to remember, which is the coupling #360 removed.

## The evaluation surface (know which entry point you're in)

Ranks are the `hand_rank_t` enum (`POKEVAL_HIGH_CARD` … `POKEVAL_FIVE_OF_A_KIND`).
The main families, each with its own tie-break path:

- **Straight 5-card:** `POKEVAL_evaluate_hand`, `POKEVAL_compare_hands`
  (`lowball` flag switches to the lowball path).
- **Best-5-of-7:** `POKEVAL_hand5_from_hand7` (holdem-style, all 7 available).
- **Omaha:** `POKEVAL_hand5_omaha` / `POKEVAL_compare_hands_omaha` — **must** use
  exactly 2 of the 4 hole cards + 3 of 5 community (all C(4,2)×C(5,3)=60 combos).
- **Wild:** `POKEVAL_evaluate_hand_wild`, `POKEVAL_hand5_from_hand7_wild`,
  `POKEVAL_compare_hands_wild` — `wild_face` is the face treated as wild.
- **Lowball:** `*_lowball` (ace is low = 1 via `lowball_value`).
- **Stud ordering:** `POKEVAL_card_bringin_lt` / `POKEVAL_suit_bringin_rank`
  (bring-in), `POKEVAL_score_stud_upcards` (≤4 up-cards, street order),
  `POKEVAL_score_visible_cards` (1–7 visible, no-peek: no suit tiebreak at n≤4 so
  equal ranks tie and force another flip).

## Wild-card tie-breaks — the historically buggy area

Wild hands have historically been the bug source here (fuzz-clean now, but this is
where regressions land). A wild card sits in the combo at its raw `face_val`
(e.g. 2) but *represents* some other card; naive high-card comparison sorts it to
the bottom and mismeasures the hand. The current design routes every same-rank
tie through `compare_wild_same_rank`, the wild-aware dispatch shared by
`update_best_wild` (best-of-7 selection) and `POKEVAL_compare_hands_wild`
(cross-player winner). Its helpers: `straight_high_wild`,
`compare_flush_tiebreak_wild`, `compare_pair_tiebreak_wild`,
`compare_kind_tiebreak_wild`, `get_group_value_wild`. Only `HIGH_CARD` uses the
`default: compare_high_cards` fallback.

- The old failure mode (fixed): a same-rank tie (two flushes, two straights, two
  straight flushes) fell through to `compare_high_cards` on the *raw* combo, where
  the wild's substituted face wasn't accounted for, so a higher hand could lose.
  If you add or change a rank's wild tie-break, route it through
  `compare_wild_same_rank` — don't reintroduce a private fallback at the call site.
- Substitution logic to mirror the non-wild code: straights fill the wild into the
  missing rung (handle the A-2-3-4-5 wheel like `update_best_5card` does); flushes
  substitute each wild to the dominant suit and pick the highest non-duplicate face.

## Change checklist

- **Run the fuzz harness — it is the safety net.** `tests/pokeval_fuzz_check.py`
  (driving `tests/test_pokeval_fuzz`) cross-checks the full variant set, including
  wild, over many seeds; a real ranking/tie-break regression shows up here as a
  mismatch. Run it before calling any pokeval change done.
- Add a targeted case to the matching unit test (`wild.c`, `straight.c`, `flush.c`,
  `lowball.c`, `omaha.c`, `bringin.c`, `sort_hand.c`, …) for the exact hand you
  fixed — the fuzzer finds regressions, the unit test pins the specific case.
- If you touched sorting or any "evaluate then broadcast" path, re-check ace
  normalization (destructive-sort trap above). The pokeval unit tests pin the
  contract in both directions: `sort_hand.c` expects `POKEVAL_ACE` back from
  `_for_eval`, while `6-card-hand.c`/`7-card-hand.c` expect `DH_CARD_ACE` from the
  selectors. If a change flips one of those, decide which contract you meant
  before editing the expectation.
- Keep evaluator-internal numbering (`POKEVAL_ACE`) out of anything that leaves the
  evaluator.
