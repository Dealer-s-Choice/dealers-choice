# Running a Dealer's Choice server

> **Warning: server hosting is experimental.**
> The dedicated server has had only limited testing. Things may break, and the
> network protocol is not final. For now, only run a public or shared server if
> you are adventurous and do not mind problems. The game client is the main,
> well-tested program; the server is new.

There are two ways to run the server: with Docker Compose, or directly as a
systemd service.

## Docker Compose

You need Docker and the Compose plugin.

Each service (server, bot, registry) is opt-in. A plain `docker compose up`
starts nothing. You choose what to run with `--profile`.

1. Copy the example settings file and edit it:

       cp env.example .env

   At a minimum, set `DC_PASSWORD`. The server needs a password before any
   client (or bot) can join.

2. Start the server in this directory:

       docker compose --profile server up -d

3. Stop everything:

       docker compose down

### Settings (`.env`)

All settings are optional except `DC_PASSWORD`. See `env.example`.

| Variable | Meaning | Default |
|---|---|---|
| `DC_PASSWORD` | Password clients must use to join. **Required.** | (none) |
| `DC_PORT` | Host port the server is published on. | `22777` |
| `DC_IMAGE` | Container image to run. | `ghcr.io/dealer-s-choice/dealers_choice:latest` |
| `DC_CONFIG_DIR` | Host folder mounted at `/dc_config` (put `server.conf` here). | (none) |
| `DC_OUTPUT_DIR` | Host folder mounted at `/dc_output` where the server writes its logs. | (none) |
| `DC_EXTRA_ARGS` | Extra command-line options for the server. | (none) |
| `DC_BOT_HOST` | Server the bot connects to. Only used by the `bot` profile. | `dealers-choice-server` |
| `DC_BOT_PORT` | Port the bot connects to. Only used by the `bot` profile. | `DC_PORT` |
| `DC_BOT_ARGS` | Extra command-line options for the bot. Only used by the `bot` profile. | (none) |
| `DC_REGISTRY_DIR` | Host folder mounted at `/dc_registry` where the registry writes `servers.json`. Only used by the `registry` profile. | (none) |
| `DC_REGISTRY_PORT` | Host port the registry is published on. Only used by the `registry` profile. | `22070` |
| `DC_REGISTRY_ARGS` | Extra command-line options for the registry (for example `--verbose`). Only used by the `registry` profile. | (none) |

The server writes a hand log and a results log automatically. They are named by
`DC_PORT` and placed under the `/dc_output` mount:

    hands-22777.txt
    results-22777.md

Naming the files by port lets you run two servers (with different `DC_PORT`
values) without their logs colliding. Set `DC_OUTPUT_DIR` to choose where they
land on the host.

The server does not read `DC_CONFIG_DIR` or `DC_OUTPUT_DIR` itself. They only
set where the host folders are mounted. Use `DC_EXTRA_ARGS` for anything else,
such as a config file or a server name:

    DC_EXTRA_ARGS=--conf /dc_config/server.conf --name "My Table"

`DC_IMAGE` defaults to the `:latest` tag, which follows the development version
(the current git trunk), not a stable release. To pin a released version, set a
version tag (replace `vX.Y.Z` with a real version from the releases page):

    DC_IMAGE=ghcr.io/dealer-s-choice/dealers_choice:vX.Y.Z

### Bots (optional)

The Compose file can also run one or more bots. Bots are not started by
default, and they do not start a server for you.

To start a server and a bot together, give both profiles:

    docker compose --profile server --profile bot up -d

To run several bots at once:

    docker compose --profile bot up -d --scale dealers-choice-bot=3

By default a bot connects to the `dealers-choice-server` service on `DC_PORT`
(so reusing a per-instance env file points the bot at that instance's server).
To point it at a different server (for example one already running, or an
external host), set `DC_BOT_HOST` and `DC_BOT_PORT`. The bot has no built-in
dependency on the server. If no server is reachable, the bot exits with an error
and is restarted, so start a server first or point the bot at a running one.

### Registry (optional)

> **Warning: the registry is experimental.**
> It is new and lightly tested, and the protocol is not final. Run a registry
> only if you are adventurous and want to host a public server directory.

The registry is a directory of public game servers. Game servers announce
themselves to it, and clients can browse the list to find internet games. You
do not need a registry for LAN play. The registry does not store the IP
addresses of clients who only browse the list.

The registry is not started by default. To start it:

    docker compose --profile registry up -d

Before you start it, set `DC_REGISTRY_DIR` to a host folder. The registry
writes the server list to `servers.json` inside that folder. A website can then
serve that file.

The registry listens on port `22070`. Change the published host port with
`DC_REGISTRY_PORT`. Open inbound TCP on that port on the registry host. To see
log output, set `DC_REGISTRY_ARGS=--verbose`.

## systemd (without Docker)

If you installed Dealer's Choice on a Linux system, you can run the server as a
systemd service. An example unit file is in
[`packaging/systemd/dealers-choice-server.service`](../packaging/systemd/dealers-choice-server.service).

1. Copy the unit file:

       sudo cp packaging/systemd/dealers-choice-server.service /etc/systemd/system/

2. Create the settings folder, then create the file
   `/etc/dealers-choice/server.env` with this content:

       DC_PASSWORD=your-password-here
       DC_EXTRA_ARGS=--conf /etc/dealers-choice/server.conf --log-game-results /var/lib/dealers-choice/game_results.md

   `DC_EXTRA_ARGS` is optional. If you do not need a `server.conf`, leave that
   line out and just set `DC_PASSWORD`. Keep the file private:

       sudo install -d /etc/dealers-choice
       sudo chmod 600 /etc/dealers-choice/server.env

3. (Optional) Put a `server.conf` at `/etc/dealers-choice/server.conf` if you
   referenced it in `DC_EXTRA_ARGS` above.

4. Enable and start it:

       sudo systemctl daemon-reload
       sudo systemctl enable --now dealers-choice-server

The service writes game results under `/var/lib/dealers-choice/`. Read its log
with `journalctl -u dealers-choice-server`.
