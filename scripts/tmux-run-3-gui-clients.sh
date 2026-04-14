#!/bin/sh
#
# Starts 3 clients in a tmux session
#
# Used for testing

set -ev

tmux new-session -d -s dealers "bash -c './dealers-choice --verbose; exec bash'" \; \
  new-window "bash -c './dealers-choice --verbose; exec bash'" \; \
  new-window "bash -c './dealers-choice --verbose; exec bash'" \; \
  attach
