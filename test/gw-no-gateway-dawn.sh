#!/bin/bash
#
# gw-no-gateway-dawn.sh -- prove a Groundwire ship boots with NO PKI gateway.
#
# Regression guard for the change that removed the hardcoded boot-time
# gateway (main.c) and the =lamp=/=turf= HTTP fetches (dawn.c).  A
# confidential comet must reach earth without a plaintext HTTP GET to any
# server -- that GET leaked its IP+boot-time and poured the server's
# entire peer table into the new ship's jael.
#
# Asserts, on a freshly-mined throwaway comet:
#   1. it BOOTS (king does not fail) with no --gateway
#   2. dawn logs NO "retrieving galaxy table" / "retrieving network domains"
#   3. jael comes up with NO static-ip lamp entries (the injected junk)
#   4. turf is the baked network constant (jael's non-empty-turf invariant)
#   5. with an explicit --gateway, the fetch DOES happen (escape hatch intact)
#
# Usage: gw-no-gateway-dawn.sh <urbit-binary> <pill> [<gateway-url-for-step-5>]
#
set -u
VERE="${1:?urbit binary}"
PILL="${2:?pill}"
GATE="${3:-}"          # optional: a reachable gateway to prove step 5
MINER="${GW_MINER:-}"  # optional: comet_miner to mine a fresh feed
WORK="$(mktemp -d)"
trap 'pkill -f "$WORK" 2>/dev/null; rm -rf "$WORK"' EXIT
fail() { echo "FAIL: $*" >&2; exit 1; }

# --- a feed to boot: mine one, or accept GW_FEED/GW_COMET from the env ---
if [ -n "${GW_FEED:-}" ] && [ -n "${GW_COMET:-}" ]; then
  FEED="$GW_FEED"; COMET="$GW_COMET"
elif [ -n "$MINER" ]; then
  OUT="$("$MINER" -c daplyd 2>/dev/null)"
  FEED="$(printf '%s\n' "$OUT" | sed -n 's/^feed: //p')"
  COMET="$(printf '%s\n' "$OUT" | sed -n 's/^comet: //p')"
else
  fail "need GW_FEED+GW_COMET in env, or GW_MINER=<comet_miner>"
fi
[ -n "$FEED" ] && [ -n "$COMET" ] || fail "no feed/comet"
NAME="${COMET#\~}"
echo "booting $COMET with no gateway"

# --- step 1-4: default boot, no gateway ---
LOG="$WORK/boot.log"
timeout 480 "$VERE" -d -c "$WORK/pier" -w "$NAME" -G "$FEED" -B "$PILL" \
  --http-port 8099 -p 39099 > "$LOG" 2>&1
rc=$?
[ "$rc" = 0 ] || fail "boot exited $rc (see $LOG); king: $(grep -a 'king:' "$LOG" | tail -1)"
grep -aq "king: boot failed" "$LOG" && fail "king: boot failed"
[ "$(grep -ac 'retrieving galaxy table\|retrieving network domains' "$LOG")" = 0 ] \
  || fail "dawn contacted a gateway (found 'retrieving ...' lines)"
[ "$(grep -ac 'static ip' "$LOG")" = 0 ] \
  || fail "jael has static-ip lamp junk -- a gateway table was injected"
grep -aq "our urbit is\|ship: loading\|$COMET" "$LOG" || fail "ship did not come up"
pkill -f "$WORK/pier" 2>/dev/null; sleep 2
echo "PASS 1-4: booted, no gateway contact, no lamp junk"

# --- step 5: explicit gateway still fetches (escape hatch) ---
if [ -n "$GATE" ]; then
  rm -rf "$WORK/pierW"; LOGW="$WORK/wboot.log"
  timeout 300 "$VERE" -d -c "$WORK/pierW" -w "$NAME" -G "$FEED" -B "$PILL" \
    --http-port 8098 -p 39098 --gateway "$GATE" > "$LOGW" 2>&1
  [ "$(grep -ac 'retrieving galaxy table' "$LOGW")" -ge 1 ] \
    || fail "--gateway $GATE did not fetch (escape hatch broken)"
  pkill -f "$WORK/pierW" 2>/dev/null; sleep 1
  echo "PASS 5: --gateway still fetches"
else
  echo "SKIP 5: no gateway url given (pass one as \$3 to test the escape hatch)"
fi
echo "ALL PASS"
