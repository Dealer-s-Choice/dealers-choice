# Config

## Server Config

The server reads
[data/server.conf](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/data/server.conf)
on startup (or the file specified via `--server-conf`).

### Password

To require players to authenticate before joining, set a password:

```
password = mysecretpassword
```

Leave the value empty (or omit the key) to allow anyone to connect without a
password.

The `DC_PASSWORD` environment variable takes precedence over `server.conf`:

```
DC_PASSWORD=mysecretpassword ./dealers-choice --server
```

Clients must also have `DC_PASSWORD` set to the same value in order to connect.

---

## Player Config

When you run the client from a terminal, you'll see the path to your config
file. You can edit settings from the Settings menu on the startup screen, or
by editing the file directly.

## Sound

```
sound.volume = 7 # You can use a range of 0 - 10
sound.notify.turn = yes # Use "no" to disable your turn audio notification
```

## Language

To use a language that is different from your system's default:

```
language = de
```

The value must match the name of a `.po` translation file, without the `.po`
ending.

You can see the available languages in the [po
directory](https://github.com/Dealer-s-Choice/dealers_choice/tree/trunk/po).
