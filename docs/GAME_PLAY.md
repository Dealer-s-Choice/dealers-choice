# Game Play

## Discard / Draw

When it is your turn, select the cards you would like to discard, then click
the "Discard" button.

[![Discarding](https://dealer-s-choice.github.io/screenshots/dealers-choice_discarding.gif)](https://dealer-s-choice.github.io/screenshots/dealers-choice_discarding.gif)

If you want to keep all your cards, click the "Discard" button without
selecting any cards.

## Player action timeouts

If a player is at their turn but doesn't select an action (e.g. bet, discard)
within 20 seconds, the client will enact a default.

If there has been no bet, the default action is "check".

If there has been a bet, the default action is "fold".

The timeout is defined in
[server.conf](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/data/server.conf).

If a player times out repeatedly, they will be disconnected from the game ('0'
to disable; see `action_timeout_max` in server.conf).

## Hotkeys

| Key | Action                  |
|-----|-------------------------|
| c   | call/check              |
| r   | raise/complete          |
| f   | fold                    |
| d   | discard                 |
| b   | bet                     |
| x   | exchange (wild cards)   |


The bet amount hotkeys correspond to the amounts configured in `bet_amounts`
in server.conf, in order, starting from key `1`. Up to 8 amounts are
supported (keys `1`–`8`). With the default configuration:

|      |   Amount   |
|------|------------|
| 1    |    100     |
| 2    |    250     |
| 3    |    500     |

|             Full Screen / Navigation                        |
|-------------------------------------------------------------|
| F11 or Alt+Enter  | toggle full-screen                      |
| ESC               | quit (with confirmation)                |

Hotkeys are not implemented for everything yet, but they are planned, as well
as [making them
configurable](https://github.com/Dealer-s-Choice/dealers-choice/issues/102).

## Stud Games (5-card, 6-card, 7-card stud)

### Bring-in bet

After the initial cards are dealt, the player with the **lowest face-up card** is required
to post a forced partial bet called the **bring-in**.

- Face value is compared ace-high (ace is highest, so twos bring in most often).
- Ties are broken by suit rank: **clubs < diamonds < hearts < spades** (clubs brings in first).
- The bring-in amount is set by `bringin_amount` in `server.conf` (default: 50).

After the bring-in is placed, the action moves clockwise. Players may:

- **Call** the bring-in amount
- **Complete** (raise to the full small bet) or raise further, subject to the normal raise limit
- **Fold**

The bring-in player acts last. If no one has completed or raised, they may **Complete** for free
(no additional cost beyond the bring-in already posted) or **Check**. The button is labeled
"Complete" rather than "Raise" or "Bet" to make this distinction clear.

### Subsequent streets

On each street after the first, the player with the **best visible hand** acts first. They
may check or bet freely — there is no forced bring-in on later streets.

### Using Wild Cards

Selecting Deuces Wild will temporarily disable the following games until it is
unselected:

* Omaha
* Texas Hold’em
* California Lowball
* 7-card No Peek

Only 2s can be wild, and only if the dealer has enabled wild cards. The server
evaluates wild hands automatically at showdown.
