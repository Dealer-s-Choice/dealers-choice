# Bot

`dealers-choice-bot` is a headless, rules-based CLI bot that connects to a
running Dealer's Choice server as a regular player. No display or audio is
required.

## Usage

The server must have a password set for the bot to join it.

```
dealers-choice-bot [options]

  --host <host>      Server host (default: 127.0.0.1)
  --port <port>      Server port (default: 22777)
  --nick <nick>      Bot nickname (default: Bot)
```

## Example

Launch a bot on the same machine that the server is running on:

```sh
DC_PASSWORD=... ./dealers-choice-bot --nick Bot1
```
