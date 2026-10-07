#!/usr/bin/env bash
# Re-runs the exporter in trace mode; whenever it dies natively, appends the
# getter that was running to native_getters.txt and tries again.
set -u
cd "$(dirname "$0")"
DOTNET=${DOTNET:-$HOME/.dotnet/dotnet}
OUT=${1:-/tmp/sts2_content_probe.json}
"$DOTNET" build -c Release -v q > /dev/null || exit 1
for i in $(seq 1 200); do
  cp native_getters.txt bin/Release/net9.0/native_getters.txt
  LOG=$(mktemp)
  STS2EXP_TRACE=1 "$DOTNET" bin/Release/net9.0/Sts2Exporter.dll --out "$OUT" > "$LOG" 2>&1
  rc=$?
  if [ $rc -eq 0 ]; then
    grep -v '^  get \|^step ' "$LOG" | tail -3
    rm -f "$LOG"
    echo "clean run after $((i - 1)) additions"
    exit 0
  fi
  last=$(grep '^  get ' "$LOG" | tail -1 | awk '{print $2}')
  rm -f "$LOG"
  if [ -z "$last" ] || grep -qx "$last" native_getters.txt; then
    echo "exporter failed (rc=$rc) outside a property getter; last getter: ${last:-none}" >&2
    exit 1
  fi
  echo "$last" >> native_getters.txt
  echo "denied $last"
done
