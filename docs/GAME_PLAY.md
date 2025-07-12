# Game Play

## Player action timeouts

If a player is at their turn but doesn't select an action (e.g. bet, discard)
within 20 seconds, the client will enact a default.

If there has been no bet, the default action is "check".

If there has been a bet, the default action is "fold".

The timeout is defined in
[server.conf](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/data/server.conf).

## Hotkeys

| Key | Action               |
|-----|----------------------|
| c   | call/check           |
| r   | raise                |
| f   | fold                 |
| d   | discard              |
| b   | bet                  |
| x   | exchange (wild cards)|


|      |   Amount   |
|------|------------|
| 1    |    100     |
| 2    |    250     |
| 3    |    500     |

Hotkeys are not implemented for everything yet, but they are planned, as well
as [making them
configurable](https://github.com/Dealer-s-Choice/dealers-choice/issues/102).

### Using Wild Cards

Only 2s can be wild, and only if the dealer has enabled wild cards.

At the end of the final betting round, if you have a 2 in your hand, you’ll
see an **"Exchange"** button and a column showing face values and suits.

1. Click on a 2 in your hand.
2. Click on a face value, a suit, or both in the column to assign that identity to your wild card.
3. Repeat for any other 2 in your hand.
4. When finished, click **"Exchange"** again to submit your changes.
   *(Note: The button text may vary if a translation is enabled.)*
