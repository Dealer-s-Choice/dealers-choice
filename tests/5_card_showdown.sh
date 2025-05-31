#!/bin/sh
set -e

# Launch the server with ---test and capture its PID
"${MESON_BUILD_ROOT}/dealerschoice" --server ---test &
SERVER_PID=$!

# Ensure the server is killed no matter what
cleanup() {
  echo "Cleaning up server..."
  kill "$SERVER_PID" 2>/dev/null || true
}
trap cleanup EXIT

# Wait for server to initialize (adjust if needed)
sleep 1

# Run the test client which connects to the server
"${MESON_BUILD_TEST_ROOT}/test_5_card_showdown"
