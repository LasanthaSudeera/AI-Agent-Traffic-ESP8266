#!/usr/bin/env bash
# AI Agent Traffic Light reporter for Claude Code.
# Mirrors the contract in ~/.claude-work/AGENTS.md, but reports as agent "claude".
#
#   traffic-light.sh working|blocked|permission|ready
#
# "working" also starts a detached refresher that re-posts every 60s, because the
# server expires a state after 120s. Any other state clears the refresher's token
# first, so a stale refresher can never overwrite blocked/permission/ready with
# working: the loop re-checks the token before every post and exits if it lost it.
# The token is written by the parent before the child spawns, so there is no window
# in which a refresher exists that a later stop cannot see.
#
# All requests are best-effort: this script never fails a hook.

set -u

ENDPOINT='http://ai-agent-status.local/api/status'
AGENT='claude'
SELF="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
STATE_DIR="$(dirname "$SELF")/.traffic-light"
TOKEN_FILE="$STATE_DIR/refresher.token"
REFRESH_SECS=60
MAX_REFRESH_SECS=7200  # stand down after 2h so a leaked refresher can't pin the light

post() {
  curl -fsS --max-time 5 -X POST -H 'Content-Type: application/json' \
    --data "{\"state\":\"$1\",\"agent\":\"$AGENT\"}" "$ENDPOINT" >/dev/null 2>&1 || true
}

current_token() {
  [ -s "$TOKEN_FILE" ] || return 1
  read -r tok < "$TOKEN_FILE" 2>/dev/null || return 1
  [ -n "${tok:-}" ] || return 1
  printf '%s' "$tok"
}

# Detached refresh loop. Re-checks its token before each post, so clearing the
# token file is enough to guarantee it never posts again.
if [ "${1:-}" = '--refresh' ]; then
  mine="${2:-}"
  [ -n "$mine" ] || exit 0
  elapsed=0
  while [ "$elapsed" -lt "$MAX_REFRESH_SECS" ]; do
    sleep "$REFRESH_SECS"
    elapsed=$((elapsed + REFRESH_SECS))
    [ "$(current_token || true)" = "$mine" ] || exit 0
    post working
  done
  [ "$(current_token || true)" = "$mine" ] && rm -f "$TOKEN_FILE"
  exit 0
fi

stop_refresher() {
  tok="$(current_token || true)"
  rm -f "$TOKEN_FILE" 2>/dev/null
  if [ -n "${tok:-}" ]; then
    for p in $(pgrep -f -- "--refresh $tok" 2>/dev/null); do
      kill "$p" 2>/dev/null
    done
  fi
  return 0
}

start_refresher() {
  tok="$(current_token || true)"
  if [ -n "${tok:-}" ] && pgrep -f -- "--refresh $tok" >/dev/null 2>&1; then
    return 0  # already refreshing
  fi
  tok="$$-$(date +%s)-${RANDOM}"
  printf '%s\n' "$tok" > "$TOKEN_FILE" || return 0
  setsid "$SELF" --refresh "$tok" </dev/null >/dev/null 2>&1 &
  return 0
}

case "${1:-}" in
  working)
    mkdir -p "$STATE_DIR" 2>/dev/null
    post working
    start_refresher
    ;;
  blocked|permission|ready)
    stop_refresher
    post "$1"
    ;;
  *)
    echo "usage: $(basename "$0") working|blocked|permission|ready" >&2
    exit 64
    ;;
esac

exit 0
