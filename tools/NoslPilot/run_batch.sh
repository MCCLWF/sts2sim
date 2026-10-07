#!/usr/bin/env bash
# Several seeds through the live pilot, one after another, each compared with
# the pilot's run without a game. One JSON line per seed in OUT/results.jsonl.
#
#   tools/NoslPilot/run_batch.sh --weights W.bin --out DIR --prefix NOSLDEV --first 5 --count 20 [--policy-seed 1]
#       [--fast Fast|Normal|Instant] [--interactive 1|0]
set -u
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${NOSL_PILOT_BUILD:-$ROOT/build}"
weights="" out="" prefix=NOSLDEV first=0 count=1 policy_seed=1 fast=Fast interactive=1 mode=shadow
while (($#)); do
    case "$1" in
        --weights) weights="$2" ;; --out) out="$2" ;; --prefix) prefix="$2" ;; --first) first="$2" ;; --count) count="$2" ;;
        --policy-seed) policy_seed="$2" ;; --fast) fast="$2" ;; --interactive) interactive="$2" ;; --mode) mode="$2" ;; *) echo "run_batch.sh: unknown option $1" >&2; exit 2 ;;
    esac
    shift 2
done
[[ -n $weights && -n $out ]] || { echo "run_batch.sh: --weights and --out are required" >&2; exit 2; }
mkdir -p -- "$out"
for ((i = first; i < first + count; i++)); do
    seed="$(printf '%s%04d' "$prefix" "$i")"
    "$BUILD/nosl_pilot" --seed "$seed" --weights "$weights" --policy-seed "$policy_seed" --out "$out/dry-$seed.jsonl" > "$out/dry-$seed.summary.json"
    "$ROOT/tools/NoslPilot/run_live.sh" --seed "$seed" --weights "$weights" --policy-seed "$policy_seed" --out "$out/run-$seed" \
        --fast "$fast" --interactive "$interactive" --mode "$mode" --timeout 900 > /dev/null 2>&1
    launcher=$?
    compare="$(python3 "$ROOT/tools/NoslPilot/compare_dry.py" "$out/run-$seed/pilot.jsonl" "$out/dry-$seed.jsonl" 2>/dev/null || true)"
    [[ -n $compare ]] || compare='{"same":false,"outcome":"no_log"}'
    printf '{"seed":"%s","launcher_exit":%d,"launcher":%s,"compare":%s}\n' "$seed" "$launcher" \
        "$(cat "$out/run-$seed/launcher.json" 2>/dev/null || echo null)" "$compare" >> "$out/results.jsonl"
done
