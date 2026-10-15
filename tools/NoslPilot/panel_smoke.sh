#!/usr/bin/env bash
# Rendered integration test. All saves and test actions belong to this isolated instance.
# Usage: tools/NoslPilot/panel_smoke.sh MODEL_DIR [full|restore|missing-model]
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
GAME="${STS2_GAME_DIR:-$HOME/snap/steam/common/.local/share/Steam/steamapps/common/Slay the Spire 2}"
INSTANCE="$ROOT/artifacts/nosl-r5-interactive/panel-instance"
TEMPLATE="${NOSL_PILOT_PROFILE_TEMPLATE:-$ROOT/artifacts/legacy/live-diff-runtime}"
MODEL="$(realpath -e -- "${1:?Pass an exported model directory}")"
MODE="${2:-full}"
[[ $MODE == full || $MODE == restore || $MODE == missing-model ]] || exit 2
OUT="${NOSL_PANEL_TEST_OUT:-$ROOT/artifacts/nosl-r5-interactive/panel-$MODE}"
BUILD="${NOSL_PILOT_BUILD:-$ROOT/build}"
mkdir -p -- "$OUT"
rm -f -- "$OUT/result.json"
"${DOTNET:-$HOME/.dotnet/dotnet}" build "$ROOT/tools/NoslPilot/NoslPilot.csproj" -c Release -p:PilotPanelTest=true --nologo > "$OUT/build.log" 2>&1
if [[ ! -x $INSTANCE/game/SlayTheSpire2 ]]; then
    mkdir -p -- "$INSTANCE/game"
    for path in "$GAME"/*; do
        [[ $(basename -- "$path") == mods || $(basename -- "$path") == mods-back ]] && continue
        cp -a --reflink=auto -- "$path" "$INSTANCE/game/"
    done
fi
if [[ ! -d $INSTANCE/data ]]; then
    mkdir -p -- "$INSTANCE/data/SlayTheSpire2" "$INSTANCE/config" "$INSTANCE/cache"
    cp -a -- "$TEMPLATE/data/SlayTheSpire2/default" "$INSTANCE/data/SlayTheSpire2/"
    [[ ! -d $TEMPLATE/config ]] || cp -a -- "$TEMPLATE/config/." "$INSTANCE/config/"
fi
if [[ $MODE != restore ]]; then
    find "$INSTANCE/data" -name 'current_run.save*' -delete
    find "$INSTANCE/data" -name 'nosl-pilot-panel.json' -delete
    find "$INSTANCE/data" -name 'nosl-pilot.jsonl' -delete
fi
MOD="$INSTANCE/game/mods/NoslPilot"
mkdir -p -- "$MOD/model"
cp -f -- "$ROOT/tools/NoslPilot/.godot/mono/temp/bin/Release/NoslPilot.dll" "$ROOT/tools/NoslPilot/NoslPilot.json" "$BUILD/libsts2nosl_pilot.so" "$MOD/"
cp -f -- "$MODEL"/* "$MOD/model/"
[[ $MODE != missing-model ]] || rm -- "$MOD/model/policy.bundle"
cd -- "$INSTANCE/game"
exec timeout --signal=TERM --kill-after=5s 300s env \
    XDG_DATA_HOME="$INSTANCE/data" XDG_CONFIG_HOME="$INSTANCE/config" XDG_CACHE_HOME="$INSTANCE/cache" \
    NOSL_PILOT_AUTORUN=0 NOSL_PANEL_TEST=1 NOSL_PANEL_TEST_MODE="$MODE" NOSL_PANEL_TEST_OUT="$OUT" \
    ./SlayTheSpire2 --windowed --resolution 1280x720 --audio-driver Dummy --force-steam=off --log-file "$OUT/godot.log" > "$OUT/stdout.txt" 2>&1
