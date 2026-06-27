# Config

## Server Config

The server reads
[data/server.conf](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/data/server.conf)
on startup (or the file specified via `--conf`).

### Password

Password authentication requires libsodium. If the build was compiled without
it, any configured password is ignored and a warning is printed at startup.

To require players to authenticate before joining, set a password:

```
password = mysecretpassword
```

Leave the value empty (or omit the key) to allow anyone to connect without a
password.

The `DC_PASSWORD` environment variable takes precedence over `server.conf`:

```
DC_PASSWORD=mysecretpassword ./dealers-choice-server
```

Clients can also also use either method for setting their password.

### Admin controls

Any client that authenticates with the server password is granted admin status
for that session. An admin can kick or ban other players during a lobby or
game via the Kick / Ban buttons in the player list.

- **Kick** — the target player is immediately disconnected and their coins are
  reset to the starting amount.
- **Ban** — same as kick, but the player's IP address is added to an in-memory
  ban list so they cannot reconnect for the duration of the server session.
  Bans are not persisted to disk and are cleared when the server restarts.

An admin cannot kick or ban themselves.

### Bet amounts

The bet amounts shown during a game (as a notched bet-amount scale) are
configured as a comma-separated list:

```
bet_amounts = list, 100, 250, 500
```

Up to 8 values are supported. The hotkeys `1`–`8` map to each amount in
order. The first value is also used to scale the coin display in the pot.

---

## Player Config

When you run the client from a terminal, you'll see the path to your config
file. You can edit settings from the Settings menu on the startup screen, or
by editing the file directly.

## Your identity key

When you connect to a server, Dealer's Choice makes a small file:

```
~/.local/share/dealers-choice/identity.key
```

(the matching per-user app-data folder on macOS and Windows).

This file is your identity. It is private, like a password. Keep it, and do not
share it.

For now the server only uses it to recognise you. It does not change how you
play yet. Later features, such as saved chip balances, will use it.

The key belongs to this computer. If you play on another computer, you get a new
key and are a different player there. A way to move your key between computers is
planned.

If you delete the file, the next connection makes a new key.

## Sound

```
sound.volume = 7 # You can use a range of 0 - 10
sound.notify.turn = yes # Use "no" to disable your turn audio notification
```

## Connection

```
connect.attempts = 6 # Number of connection attempts before giving up
```

## Server browser

The connect screen lists internet servers by asking the registries in
`common.conf` (see the [registry guide](REGISTRY.md)). To stop the client from
contacting any registry, set:

```
registry_browser = no
```

The default is `yes`. You can also change this on the Settings screen, or turn
it off for one run from the command line:

```
dealers-choice --disable-registry-browser
```

This only affects the internet server list. LAN discovery is separate and is
not changed by this setting.

## Language

To use a language that is different from your system's default:

```
language = de
```

The value must match the name of a `.po` translation file, without the `.po`
ending.

You can see the available languages in the [po
directory](https://github.com/Dealer-s-Choice/dealers_choice/tree/trunk/po).

## Game-play hotkeys

In-game keys are set in your `player.conf`; see **[keys.md](keys.md)** for the
full list and how to change them.
