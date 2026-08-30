#!/bin/bash
# Two GUI clients, one private Xvfb display each: identical window geometry on a
# shared display makes clicks ambiguous, so give each client its own display.
set -u
B=/home/andy/src/DealersChoice/dealers_choice/_build_dealers_choice
for d in 99 98; do Xvfb :$d -screen 0 1920x1080x24 >/dev/null 2>&1 & done
sleep 2
cd "$B" || exit 1
for d in 99 98; do
  DISPLAY=:$d ASAN_OPTIONS=verify_asan_link_order=0 \
    ./dealers-choice --host 127.0.0.1 --port 22777 > /tmp/dc_gui/c$d.log 2>&1 &
done
sleep 7
for d in 99 98; do
  W=$(DISPLAY=:$d xdotool search --onlyvisible "" 2>/dev/null | tail -1)
  echo ":$d window $W"
  echo "$W" > /tmp/dc_gui/win.$d
  DISPLAY=:$d xdotool mousemove --window "$W" 858 226 click 1
done
sleep 5
echo "both connected"
