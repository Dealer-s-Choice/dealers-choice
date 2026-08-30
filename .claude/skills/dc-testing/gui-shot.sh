#!/bin/bash
# Root-capture + crop to the client window. Per-window `import -window <id>`
# returns solid black once the game's render context is live; capturing root and
# cropping to the window geometry works on every screen.
set -u
D="$1"; OUT="$2"
W=$(cat /tmp/dc_gui/win.$D)
GEO=$(DISPLAY=:$D xdotool getwindowgeometry "$W")
POS=$(echo "$GEO" | awk '/Position/{print $2}' | tr ',' '+')
SIZE=$(echo "$GEO" | awk '/Geometry/{print $2}')
DISPLAY=:$D import -window root /tmp/dc_gui/_full.png 2>/dev/null
magick /tmp/dc_gui/_full.png -crop "${SIZE}+${POS//+/+}" +repage -resize 60% "$OUT" 2>/dev/null
echo "$OUT ($SIZE at $POS)"
