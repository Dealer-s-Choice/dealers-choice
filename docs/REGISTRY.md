# Server registry

`dealers-choice-registry` is an optional directory server. Game servers can
publish themselves to it, and clients can browse the list to find internet
games (the LAN-only equivalent is built into the game and needs no registry).

> **The registry is currently in maintenance mode until further notice.** No
> public registry is being run. The code is kept building and tested, but it is
> not being developed further for now.

> **Status: experimental.** Like the dedicated server, the registry is new and
> lightly tested. Run one only if you are comfortable with that.

> ⚠️ The server and networking code was primarily written by an LLM and has
> not been fully reviewed by a human network security expert. Any instructions
> regarding running the server should take that into account. This project
> attempts to use rigorous testing methods and anticipate security issues but,
> like any project, cannot guarantee the programs here are free of potential
> vulnerabilities.
>
> A LAN or other trusted network is the intended use. Hosting a registry or a
> game server on the public internet is lower-risk if you keep it to people you
> know, but it is at your own risk until the code has had an independent
> review — and a LAN is not a hard boundary either (a compromised device or a
> guest on your wifi is already inside it).

## What it does

- A game server sends a short "I'm here" message (its name, port, player count)
  to the registry, and repeats it as a heartbeat.
- Before listing a server, the registry **connects back** to the advertised
  address and checks it really is a Dealer's Choice server. A listing you can't
  actually connect to never appears.
- Listings expire if the server stops sending heartbeats.
- The registry writes the current list to a JSON file (`--json <path>`) that a
  website can serve, and answers list requests from clients.

## Building it

The registry is **not built or installed by default** — most people running the
game never host a directory server. Turn it on at configure time:

```
meson setup _build -Dregistry=true
```

(The wire parsers it shares with the client and server are always built and
tested; only the `dealers-choice-registry` program is gated.)

## Running it

```
dealers-choice-registry --json /path/to/servers.json --verbose
```

The port defaults to 22070; pass `--port` to change it. Open inbound TCP on
that port on the registry host.
**Game servers need no extra firewall change** — the verification connects back
to the game port players already use.

## Configuring which registries to use

The list of registries lives in **`common.conf`** (in the data directory),
read by both the game client (to show internet servers on the connect screen)
and the headless server (to publish itself):

```ini
# common.conf
registry = registry.example.org
registry = 203.0.113.5, 22071
```

One registry per line (dnsmasq-style): the host is the value, with an optional
port as a comma attribute (default 22070). Repeat the line for more registries.
The headless server publishes to every listed registry unless started with
`--disable-publish`. Leave it commented out for LAN-only play.

The client browses these registries by default. To stop the client from
contacting any registry (it will then show only LAN servers), set
`registry_browser = no` in `player.conf`, change it on the Settings screen, or
run the client with `--disable-registry-browser`. See [CONFIG.md](CONFIG.md).

## Privacy — what is and isn't stored

The registry is deliberate about addresses:

- **Players who browse the server list:** their IP address is **not stored**.
  It is used only to send the reply back, and is never written to the server
  list or the JSON file. It may appear in the registry's own log only when
  `--verbose` is enabled, and even then it is not saved anywhere by the program.
- **Game servers that publish:** the server's address **is stored and shown**
  publicly (in the list, the JSON file, and any webpage). This is necessary —
  the whole purpose of a directory is to tell players where the servers are.
  A server operator opts into this by publishing; publishing can be turned off
  (the game server's `--disable-publish` option), in which case nothing about
  that server is sent to any registry.
- The registry **never sees the connection between a player and a game server**.
  Those connections do not pass through the registry, so it cannot and does not
  collect players' addresses from gameplay.

In short: a publishing server's address is public by listing (the operator's
choice); a browsing player's address is not collected.
