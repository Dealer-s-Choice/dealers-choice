#!/bin/bash
# Toggle tc netem impairment on the loopback interface to test the wire under
# WAN-like latency and loss. Needs sudo (tc requires CAP_NET_ADMIN). Affects ALL
# loopback traffic on the machine until removed — always run `netem.sh off` after.
#
# Usage:
#   netem.sh on   [delay_ms] [jitter_ms] [loss_pct]   # defaults: 60 12 0.5
#   netem.sh off
#   netem.sh status
#
# Pair it with the soak (real bots, normal timeouts), NOT the deterministic
# game_logic tests (DC_TEST short timeouts give spurious failures under latency):
#
#   scripts/netem.sh on 60 12 0.5
#   DC_BUILD=$PWD/_build_asan scripts/soak.sh        # judge by the log
#   scripts/netem.sh off
DEV=lo

case "${1:-status}" in
  on)
    delay="${2:-60}"; jitter="${3:-12}"; loss="${4:-0.5}"
    sudo tc qdisc replace dev "$DEV" root netem delay "${delay}ms" "${jitter}ms" loss "${loss}%"
    echo "netem on $DEV: delay ${delay}ms +/- ${jitter}ms, loss ${loss}%"
    tc qdisc show dev "$DEV"
    ;;
  off)
    sudo tc qdisc del dev "$DEV" root 2>/dev/null
    echo "netem off $DEV"
    tc qdisc show dev "$DEV"
    ;;
  status)
    tc qdisc show dev "$DEV"
    ;;
  *)
    echo "usage: $0 {on [delay_ms] [jitter_ms] [loss_pct] | off | status}" >&2
    exit 1
    ;;
esac
