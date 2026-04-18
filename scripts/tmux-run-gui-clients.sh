#!/bin/sh
#
# 2026
#
# Starts N clients in a tmux session (default: 3)
#
# Used for testing

set -ev

n=${1:-3}

tmux new-session -d -s dctest "bash -c './dealers-choice --auto-connect --verbose; exec bash'"
i=1
while [ "$i" -lt "$n" ]; do
  tmux new-window "bash -c './dealers-choice --auto-connect --verbose; exec bash'"
  i=$((i + 1))
done
tmux attach
