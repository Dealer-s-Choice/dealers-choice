#!/bin/bash
# Kill from a script file, never an inline pgrep -f: an inline command line that
# also mentions the binary matches ITSELF and kills the calling shell.
for p in $(pgrep -f "dealers-choice --host"); do kill "$p" 2>/dev/null; done
for p in $(pgrep -f "Xvfb :9"); do kill "$p" 2>/dev/null; done
sleep 1
echo "remaining clients: $(pgrep -cf 'dealers-choice --host')"
echo "remaining xvfb:    $(pgrep -cf 'Xvfb :9')"
