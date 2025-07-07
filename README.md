[![Linux](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/linux.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/linux.yml)
[![MacOS](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/macos.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/macos.yml)
[![Windows](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/windows.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/windows.yml)
[![CodeQL Advanced](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/codeql.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/codeql.yml)

- [Dealer's Choice](#dealer's-choice)
  - [License](#license)
  - [Project Links](#project-links)
  - [Current state](#current-state)
    - [Games available](#games-available)
  - [Game play](#game-play)
    - [Player action timeouts](#player-action-timeouts)
    - [Hotkeys](#hotkeys)
    - [Computer Opponents](#computer-opponents)
  - [Server](#server)
  - [Client](#client)
  - [Player Config](#player-config)
  - [Dedicated Servers](#dedicated-servers)
  - [Building](#building)
  - [Translating](#translating)
  - [Note on AppImages](#note-on-appimages)

# Dealer's Choice

Online Multiplayer Stud and Draw Poker, where the deal rotates and each new
game allows a different player to select the variant.

## License

This project is licensed under the MIT License – see the
[LICENSE](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/LICENSE)
file for details.

## Project Links

* [GitHub Repo](https://github.com/Dealer-s-Choice/dealers_choice)
* [Screenshots](https://dealer-s-choice.github.io/screenshots/)
* [Game Results](https://dealer-s-choice.github.io/game_results.html)
* [Discussions](https://github.com/Dealer-s-Choice/dealers_choice/discussions)
* [Bug Reporting](https://github.com/Dealer-s-Choice/dealers_choice/issues)
* [Releases](https://github.com/Dealer-s-Choice/dealers_choice/releases)

## Current state

The game is in alpha so you may encounter bugs.

*Dealer's Choice* builds on Windows in an MSYS2 environment but [there is not an installer
available yet](https://github.com/Dealer-s-Choice/dealers_choice/issues/68).

### Games available

* [5-card single and double draw](https://en.wikipedia.org/wiki/Draw_poker)
* 5-card showdown (Similar to 5-card draw, but with no discard/draw round)
* [5-card stud](https://en.wikipedia.org/wiki/Five-card_stud) (not all the rules have been implemented yet, such as the bring-in bet)
* [7-card stud](https://en.wikipedia.org/wiki/Seven-card_stud) (not all the rules have been implemented yet, such as the bring-in bet)

## Game play

### Player action timeouts

If there has been no bet, the default action is "check".

If there has been a bet, the default actions is "fold".

The duration of the timeouts are defined in
[server.conf](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/data/server.conf).

### Hotkeys

| Key  |   Action   |
|------|------------|
| c    | call/check |
| r    | raise      |
| f    | fold       |
| d    | discard    |
| b    | bet        |

|      |   Amount   |
|------|------------|
| 1    |    100     |
| 2    |    250     |
| 3    |    500     |


### Computer Opponents

There are no bots or computer opponents implemented yet.

## Server

The server uses port 22777 by default. To run it:

    ./dealers-choice --server

There is not a native option to run the server as a daemon (future
implementation is planned). If you wish to daemonize the server, you can use
the [docker-compose
file](https://github.com/Dealer-s-Choice/dealers_choice/tree/trunk/docker) or
[tmux](https://coderwall.com/p/purqma/use-tmux-for-a-poor-man-s-daemon).

## Client

To run the client, simply run the binary without any arguments. A game can not
be started until there is more than one player connected.

## Player Config

See [docs/CONFIG.md](docs/CONFIG.md) for details.

## Dedicated Servers

There are no dedicated public servers yet.

## Building

See [BUILD.md](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/BUILD.md)

## Translating

See [docs/TRANSLATE.md](docs/TRANSLATE.md)

## Note on AppImages

If you're a Linux user, you can download the AppImage from the [Releases
page](https://github.com/Dealer-s-Choice/dealers_choice/releases), or install
it with [AppMan](https://github.com/ivan-hc/AM).
