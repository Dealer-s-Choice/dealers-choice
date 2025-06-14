[![Linux](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/linux.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/linux.yml)
[![MacOS](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/macos.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/macos.yml)
[![Windows](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/windows.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/windows.yml)
[![CodeQL Advanced](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/codeql.yml/badge.svg)](https://github.com/Dealer-s-Choice/dealers_choice/actions/workflows/codeql.yml)

# Dealer's Choice

Multiplayer Stud and Draw Poker

## Current state

Playable but you're likely to encounter bugs.

## Server

The server uses port 22777 by default. To run it:

    ./dealers-choice --server

There is not a native option to run the server as a daemon (future
implementation is planned). If you wish to daemonize the server, you can use
[tmux](https://coderwall.com/p/purqma/use-tmux-for-a-poor-man-s-daemon).

## Client

To run the client, simply run the binary without any arguments.

## Config

When you run the client, you'll see the path to your config file. You will
need to manually edit the file, as there is not yet a way to make changes from
the gui.

## Hotkeys

### Player actions

| Key  |   Action   |
|------|------------|
| c    | call/check |
| r    | raise      |
| f    | fold       |
| d    | discard    |
| b    | bet        |

## Building

See [BUILD.md](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/BUILD.md)
