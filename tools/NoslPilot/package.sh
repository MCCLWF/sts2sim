#!/usr/bin/env bash
# Package an already exported policy bundle. Export is the only step that needs Python.
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
MODEL="$(realpath -e -- "${1:?Pass an exported R5 model directory}")"
OUT="${2:?Pass an output directory}"
[[ $(uname -m) == x86_64 && $(uname -s) == Linux ]] || { echo 'This package targets Linux x86_64.' >&2; exit 2; }
for file in policy.bundle combat.bin base.bin strategy.bin table.tsv readiness.tsv; do
    [[ -s $MODEL/$file ]] || { echo "Missing model file: $file" >&2; exit 2; }
done
mkdir -p -- "$OUT"
OUT="$(realpath -- "$OUT")"
BUILD="${NOSL_PILOT_BUILD:-$ROOT/artifacts/nosl-steam-runtime/build}"
if [[ -z ${NOSL_PILOT_BUILD:-} ]]; then
    SDK="${STS2_STEAM_SDK:-$ROOT/artifacts/steam-sdk}"
    bash "$ROOT/tools/NoslPilot/prepare_steam_sdk.sh" "$SDK"
    SDK="$(realpath -- "$SDK")"
    cmake -S "$ROOT" -B "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/SteamRuntime.cmake" \
        -DSTS2_STEAM_SDK="$SDK" -DSTS2_PILOT_C_ABI_ONLY=ON
fi
cmake --build "$BUILD" --target sts2nosl_pilot -j "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
bash "$ROOT/tools/NoslPilot/check_steam_abi.sh" "$BUILD/libsts2nosl_pilot.so"
"${DOTNET:-$HOME/.dotnet/dotnet}" build "$ROOT/tools/NoslPilot/NoslPilot.csproj" -c Release -p:PilotPanelTest=false --nologo
STAGE="$(mktemp -d -- "$OUT/.r5-package.XXXXXX")"
trap 'rm -rf -- "$STAGE"' EXIT
DEST="$STAGE/NoslPilot"
mkdir -p -- "$DEST/model"
cp -- "$ROOT/tools/NoslPilot/.godot/mono/temp/bin/Release/NoslPilot.dll" "$ROOT/tools/NoslPilot/NoslPilot.json" \
    "$ROOT/tools/NoslPilot/README.md" "$BUILD/libsts2nosl_pilot.so" "$DEST/"
for file in policy.bundle combat.bin base.bin strategy.bin table.tsv readiness.tsv; do
    cp -- "$MODEL/$file" "$DEST/model/"
done
# Optional: the combat advantage network of a bundle with a `combat_value` entry (R10 and later).
[[ ! -s $MODEL/combat_value.bin ]] || cp -- "$MODEL/combat_value.bin" "$DEST/model/"
(cd -- "$DEST" && sha256sum NoslPilot.dll NoslPilot.json README.md libsts2nosl_pilot.so model/* > SHA256SUMS)
tar -C "$STAGE" -czf "$OUT/NoslPilot-R5-linux-x86_64.tar.gz" NoslPilot
# A package owned by this script only; preserve an older output for easy rollback.
if [[ -e $OUT/NoslPilot ]]; then
    mv -- "$OUT/NoslPilot" "$OUT/NoslPilot.previous.$(date +%s%N)"
fi
mv -- "$DEST" "$OUT/NoslPilot"
echo "Package: $OUT/NoslPilot-R5-linux-x86_64.tar.gz"
