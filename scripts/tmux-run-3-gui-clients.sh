#!/bin/sh
#
# 2026
#
# Starts 3 clients in a tmux session
#
# Used for testing

set -ev

tmux new-session -d -s dctest "bash -c './dealers-choice --verbose; exec bash'" \; \
  new-window "bash -c './dealers-choice --verbose; exec bash'" \; \
  new-window "bash -c './dealers-choice --verbose; exec bash'" \; \
  attach
