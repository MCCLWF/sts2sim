#!/usr/bin/env bash
# One whole run of the NoSL pilot in an isolated, headless instance of the
# shipped game (docs/plans/2026-10-04-nosl-q5a-live-pilot.md).
#
#   tools/NoslPilot/run_live.sh --seed NOSLDEV0000 --weights artifacts/nosl-diag/weights/q4b-h1.bin \
#       --out artifacts/nosl-q5a/run-NOSLDEV0000 [--policy-seed 1] [--greedy 0] [--fast Fast|Normal|Instant]
#       [--interactive 1|0] [--mode shadow|direct] [--timeout 900] [--control FILE] [--quit 1|0]
#
# --mode shadow: the native pilot's shadow run supplies the observation and the
# game-side encoder is compared with it. --mode direct: no shadow; the mod
# encodes what the game shows and the actor decides on those tables.
#
# --control FILE: the pilot reads its last non-empty line before every decision (pause, resume, takeover; see
# Entry.cs). --quit 0 leaves the game running when the pilot ends; the launcher then stops it at the timeout.
#
# The default is the game's Fast speed in its normal interactive mode, about
# two and a half minutes for an act-1 run. `--fast Instant --interactive 0`
# (no waits at all, the mode of the game's own smoke test) is four times
# faster and fine for batches, but one event's cosmetic loop (Punch Off)
# crashes the game in it.
#
# The instance is a copy of the game under artifacts/nosl-q5a/instance with
# its own data, config and cache directories. The user's installation, mods
# and saves are only read (to make the copy); nothing is installed into them.
# The copy takes about 2 GiB once. The profile template (a settings save with
# mods enabled) is the one earlier live checks left in artifacts/legacy.
#
# Exit code: 0 the run ended in the game where the pilot's run ended; 1 the
# pilot stopped (mismatch, timeout, error); 2 the launcher could not start;
# 3 the launcher killed the game (wall-clock timeout or low memory).
#
# A game process that dies of a signal while it is still loading, before the
# pilot's first step, is started again (twice at most). Each such crash keeps
# its logs as startup-crash-N.* and is counted in launcher.json
# (`startup_crashes`): it is a fault of that start, reported, not a result.
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
GAME="${STS2_GAME_DIR:-$HOME/snap/steam/common/.local/share/Steam/steamapps/common/Slay the Spire 2}"
INSTANCE="${NOSL_PILOT_INSTANCE:-$ROOT/artifacts/nosl-q5a/instance}"
TEMPLATE="${NOSL_PILOT_PROFILE_TEMPLATE:-$ROOT/artifacts/legacy/live-diff-runtime}"
BUILD="${NOSL_PILOT_BUILD:-$ROOT/build}"
MOD_DLL="$ROOT/tools/NoslPilot/.godot/mono/temp/bin/Release/NoslPilot.dll"

seed="" weights="" out="" policy_seed=1 greedy=0 fast=Fast interactive=1 mode=shadow timeout=900 min_free_mib=2500 control="" quit=1
while (($#)); do
    case "$1" in
        --seed) seed="$2" ;; --weights) weights="$2" ;; --out) out="$2" ;; --policy-seed) policy_seed="$2" ;;
        --greedy) greedy="$2" ;; --fast) fast="$2" ;; --interactive) interactive="$2" ;; --mode) mode="$2" ;; --timeout) timeout="$2" ;; --min-free-mib) min_free_mib="$2" ;;
        --control) control="$2" ;; --quit) quit="$2" ;;
        *) echo "run_live.sh: unknown option $1" >&2; exit 2 ;;
    esac
    shift 2
done
[[ -n $seed && -n $weights && -n $out ]] || { echo "run_live.sh: --seed, --weights and --out are required" >&2; exit 2; }
weights="$(realpath -e -- "$weights")" || exit 2
[[ -f $MOD_DLL ]] || { echo "run_live.sh: build the mod first: dotnet build tools/NoslPilot/NoslPilot.csproj -c Release" >&2; exit 2; }
[[ -f $BUILD/libsts2nosl_pilot.so ]] || { echo "run_live.sh: $BUILD/libsts2nosl_pilot.so is missing (cmake target sts2nosl_pilot)" >&2; exit 2; }
[[ -x $GAME/SlayTheSpire2 ]] || { echo "run_live.sh: no game at $GAME (set STS2_GAME_DIR)" >&2; exit 2; }
free_mib() { awk '/MemAvailable/ {print int($2 / 1024)}' /proc/meminfo; }
(( $(free_mib) >= min_free_mib )) || { echo "run_live.sh: only $(free_mib) MiB of memory available, $min_free_mib wanted" >&2; exit 2; }

# The game, copied once. The user's own mods are left out of the copy.
if [[ ! -x $INSTANCE/game/SlayTheSpire2 ]]; then
    mkdir -p -- "$INSTANCE"
    rm -rf -- "$INSTANCE/game.partial"
    mkdir -- "$INSTANCE/game.partial"
    (cd -- "$GAME" && find . -mindepth 1 -maxdepth 1 ! -name mods ! -name mods-back -print0) |
        (cd -- "$GAME" && xargs -0 -I{} cp -a --reflink=auto -- {} "$INSTANCE/game.partial/")
    mv -- "$INSTANCE/game.partial" "$INSTANCE/game"
fi
mkdir -p -- "$INSTANCE/game/mods/NoslPilot"
cp -f -- "$MOD_DLL" "$ROOT/tools/NoslPilot/NoslPilot.json" "$BUILD/libsts2nosl_pilot.so" "$INSTANCE/game/mods/NoslPilot/"

# A profile of its own.
if [[ ! -d $INSTANCE/data/SlayTheSpire2/default ]]; then
    [[ -f $TEMPLATE/data/SlayTheSpire2/default/1/settings.save ]] || { echo "run_live.sh: no profile template at $TEMPLATE" >&2; exit 2; }
    mkdir -p -- "$INSTANCE/data/SlayTheSpire2" "$INSTANCE/config" "$INSTANCE/cache"
    cp -a -- "$TEMPLATE/data/SlayTheSpire2/default" "$INSTANCE/data/SlayTheSpire2/"
    [[ ! -d $TEMPLATE/config ]] || cp -a -- "$TEMPLATE/config/." "$INSTANCE/config/"
fi

mkdir -p -- "$out"
out="$(realpath -- "$out")"
rm -f -- "$out/pilot.jsonl" "$out/godot.log" "$out/launcher.json"
began=$(date +%s)
killed=""
peak_kib=0
startup_crashes=0
status=0
for attempt in 1 2 3; do
    rm -f -- "$out/pilot.jsonl"
    (
        cd -- "$INSTANCE/game"
        exec env DISPLAY= WAYLAND_DISPLAY= \
            XDG_DATA_HOME="$INSTANCE/data" XDG_CONFIG_HOME="$INSTANCE/config" XDG_CACHE_HOME="$INSTANCE/cache" \
            NOSL_PILOT_AUTORUN=1 NOSL_PILOT_SEED="$seed" NOSL_PILOT_WEIGHTS="$weights" NOSL_PILOT_POLICY_SEED="$policy_seed" \
            NOSL_PILOT_GREEDY="$greedy" NOSL_PILOT_FAST="$fast" NOSL_PILOT_INTERACTIVE="$interactive" NOSL_PILOT_MODE="$mode" NOSL_PILOT_LOG="$out/pilot.jsonl" NOSL_PILOT_QUIT="$quit" NOSL_PILOT_CONTROL="$control" \
            ./SlayTheSpire2 --headless --disable-vsync --max-fps 0 --force-steam=off --log-file "$out/godot.log"
    ) </dev/null >"$out/stdout.txt" 2>&1 &
    pid=$!
    while kill -0 "$pid" 2>/dev/null; do
        rss=$(awk '/VmRSS/ {print $2}' "/proc/$pid/status" 2>/dev/null || echo 0)
        (( ${rss:-0} > peak_kib )) && peak_kib=$rss
        if (( $(date +%s) - began > timeout )); then killed="timeout"; fi
        if (( $(free_mib) < 600 )); then killed="low_memory"; fi
        if [[ -n $killed ]]; then kill -TERM "$pid" 2>/dev/null || true; sleep 3; kill -KILL "$pid" 2>/dev/null || true; break; fi
        sleep 1
    done
    status=0
    wait "$pid" 2>/dev/null || status=$?
    # The game process died of a signal before the pilot took a single step (a crash while the game was still
    # loading; nothing was played): keep its logs and start once more. Anything later is a result, never retried.
    if [[ -z $killed ]] && (( status >= 128 && attempt < 3 )) && ! grep -q '"step":' "$out/pilot.jsonl" 2>/dev/null; then
        startup_crashes=$((startup_crashes + 1))
        for name in godot.log stdout.txt pilot.jsonl; do
            [[ ! -f $out/$name ]] || mv -f -- "$out/$name" "$out/startup-crash-$startup_crashes.$name"
        done
        echo "run_live.sh: the game exited with status $status before the run began; starting it again ($startup_crashes)" >&2
        continue
    fi
    break
done
printf '{"seed":"%s","exit":%d,"killed":"%s","wall_s":%d,"peak_rss_mib":%d,"startup_crashes":%d}\n' \
    "$seed" "$status" "$killed" "$(( $(date +%s) - began ))" "$(( peak_kib / 1024 ))" "$startup_crashes" | tee "$out/launcher.json"
[[ -z $killed ]] || exit 3
grep -q '"event":"end".*"outcome":"\(death\|victory\)"' "$out/pilot.jsonl" 2>/dev/null && exit 0
exit 1
