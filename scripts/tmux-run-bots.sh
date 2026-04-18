#!/bin/sh
#
# 2026
#
# Starts N bots in a tmux session (default: 1)
#
# Used for testing

set -ev

n=${1:-1}

tmux new-session -d -s dcbots "bash -c './dealers-choice-bot --nick bot1; exec bash'"
i=2
while [ "$i" -le "$n" ]; do
  tmux new-window "bash -c './dealers-choice-bot --nick bot$i; exec bash'"
  i=$((i + 1))
done
tmux attach
