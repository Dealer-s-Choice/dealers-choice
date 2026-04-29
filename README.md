[![Linux](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/linux.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/linux.yml)
[![MacOS](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/macos.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/macos.yml)
[![msys2](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/msys2.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/msys2.yml)
[![msvc](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/msvc.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/msvc.yml)
[![CodeQL Advanced](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/codeql.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/codeql.yml)

# Dealer's Choice

Dealer's Choice is a cross-platform, networked multiplayer poker game
supporting draw, stud, and community-card variants, including Texas Hold'em
and Omaha, with optional wild cards. The deal rotates around the table, and
the dealer chooses the game before each hand.

## License

This project is licensed under the MIT License – see the
[LICENSE](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/LICENSE)
file for details.

## Project Links

* [GitHub Repo](https://github.com/Dealer-s-Choice/dealers_choice)
* [Game Play](docs/GAME_PLAY.md)
* [Screenshots](https://dealer-s-choice.github.io/screenshots/)
* [Game Results](https://dealer-s-choice.github.io/game_results.html)
* [Discussions](https://github.com/Dealer-s-Choice/dealers_choice/discussions)
* [Bug Reporting](https://github.com/Dealer-s-Choice/dealers_choice/issues)
* [Releases](https://github.com/Dealer-s-Choice/dealers_choice/releases/latest)

## Current state

The game is in alpha so you may encounter bugs.

*Dealer's Choice* builds on Windows in an MSYS2 environment. [Release
versions](https://github.com/Dealer-s-Choice/dealers-choice/releases/latest) are
available as of v0.0.10. There is a development snapshot installer available
on [the snapshots
page](https://github.com/Dealer-s-Choice/dealers-choice/releases/tag/snapshot).


### Games available

* [5-card single and double draw](https://en.wikipedia.org/wiki/Draw_poker)
* 5-card showdown (Similar to 5-card draw, but with no discard/draw round)
* [5-card stud](https://en.wikipedia.org/wiki/Five-card_stud) (not all the rules have been implemented yet, such as the bring-in bet)
* [6-card stud](https://en.wikipedia.org/wiki/Stud_poker#Six-card_stud) (not all the rules have been implemented yet, such as the bring-in bet)
* [7-card stud](https://en.wikipedia.org/wiki/Seven-card_stud) (not all the rules have been implemented yet, such as the bring-in bet)
* [California lowball](https://en.wikipedia.org/wiki/Lowball_(poker))
* [Omaha](https://en.wikipedia.org/wiki/Omaha_hold_%27em)
* [Texas Hold'em](https://en.wikipedia.org/wiki/Texas_hold_%27em)
* [7-card no peek](https://youtu.be/0r6BI3NZsvo?si=wlBftL25LLQcL8C7)

Most of the games above can be played with the [Deuces (Twos) Wild
option](https://dealer-s-choice.github.io/docs/GAME_PLAY.html#using-wild-cards)
enabled.

### Computer Opponents

A headless rules-based bot (`dealers-choice-bot`) is available. See
[docs/BOT.md](docs/BOT.md) for usage.

## Server

The server uses port 22777 by default. To run it:

    ./dealers-choice --server

There is not a native option to run the server as a daemon. If you wish to
daemonize the server, you can use the [docker-compose
file](https://github.com/Dealer-s-Choice/dealers_choice/tree/trunk/docker) or
[tmux](https://coderwall.com/p/purqma/use-tmux-for-a-poor-man-s-daemon).

See [docs/CONFIG.md](docs/CONFIG.md) for server configuration options including
password protection.

## Client

To run the client, simply run the binary without any arguments. A game can not
be started until there is more than one player connected.

## Hardware Requirements

Dealer's Choice is a lightweight 2D application and runs on modest hardware.

| Component | Minimum |
|-----------|---------|
| OS        | Linux (kernel 4.x+), macOS 14, Windows 10 |
| CPU       | 1 GHz single-core |
| RAM       | 2 GB |
| Graphics  | Any display output supported by SDL2 |
| Storage   | 100 MB |
| Network   | LAN or internet connection to reach a server |
| Audio     | Optional — any audio output device |
| Display   | 1280 × 720 minimum, 1920 × 1080 recommended |

The server (`--server` mode) has no graphics, audio, or display
requirements and can run on any architecture where SDL2 is available,
including headless systems.

## Player Config

See [docs/CONFIG.md](docs/CONFIG.md) for details.

## Dedicated Servers

There are no dedicated public servers yet (See
[Project](https://github.com/orgs/Dealer-s-Choice/projects/1) for road map).

## Building

See [BUILD.md](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/BUILD.md)

## Translating

See [docs/TRANSLATE.md](docs/TRANSLATE.md)

## Note on AppImages

If you're a Linux user, you can download the AppImage from the [Releases
page](https://github.com/Dealer-s-Choice/dealers_choice/releases/latest), or install
it with [AppMan](https://github.com/ivan-hc/AM).

## Libraries

These are some of the libraries used by Dealer's Choice. The source code is
bundled with this project and there is no need to install these dependencies.

* [miniaudio](https://miniaud.io/)
* [Pokeval](https://github.com/Dealer-s-Choice/pokeval)
* [deckhandler](https://github.com/Dealer-s-Choice/deckhandler)
* [pcg-c-basic](https://www.pcg-random.org/using-pcg-c-basic.html)
* [canfigger](https://github.com/andy5995/canfigger/)
