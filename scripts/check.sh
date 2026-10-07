#!/usr/bin/env bash
# The acceptance gate for merging into main: build, unit tests, then the
# differential checks against the real game.
#
#   scripts/check.sh              build + ctest + L2 (20 seeds x 5 characters x A0/A10)
#                                 + L3 (20 seeds x 2 policies x A0/A10 first-floor combats)
#                                 + L4 (20 seeds x 2 policies runs to the act 1 boss, with and without Neow;
#                                   Unported, truncated traces, and differences all fail)
#                                 + two strict first-act relic paths with per-decision snapshot restore
#                                 + strict Ironclad continuations through the Act 2 boss
#                                 + one strict Ironclad route through the final victory
#                                 + two more strict Ironclad routes, including a long Anger fight
#                                 + a forced Pael's Wing sacrifice path
#   scripts/check.sh --quick      build + ctest + smoke only (no game install needed)
#   scripts/check.sh --seeds 200  larger L2 and L3 runs
#
# The L2 and L3 steps need the game install (STS2_GAME_DIR, see AGENTS.md) and the
# .NET 9 SDK (DOTNET, default ~/.dotnet/dotnet).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOTNET="${DOTNET:-$HOME/.dotnet/dotnet}"
QUICK=0
SEEDS=20
while [[ $# -gt 0 ]]; do
  case "$1" in
    --quick) QUICK=1 ;;
    --seeds) SEEDS="$2"; shift ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

step() { printf '\n== %s\n' "$1"; }

step "C++ build"
python3 "$ROOT/tools/gen/gen_hook_frames.py" --check
python3 "$ROOT/tools/gen/gen_dispatch.py" --check
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build" -j "${STS2_BUILD_JOBS:-2}"

step "unit tests (L0/L1 golden vectors, content tables)"
ctest --test-dir "$ROOT/build" --output-on-failure

step "v0.111.0 non-hero relic catalog coverage"
python3 "$ROOT/tests/coverage_nonhero_relics.py"

step "smoke: agents play whole runs (no error, no hang; unported content may stop a run)"
SMOKE_SEEDS="$(python3 -c "print(','.join('SMOKE%04d' % i for i in range(100)))")"
for flags in "" "--no-neow 1" "--no-neow 1 --ported-only 1 --greedy 1"; do
  # shellcheck disable=SC2086
  "$ROOT/build/sts2sim_cli" smoke --seeds "$SMOKE_SEEDS" $flags | tail -1
done

if [[ $QUICK -eq 1 ]]; then
  printf '\nquick check passed (differential checks skipped)\n'
  exit 0
fi

step "oracle build"
"$DOTNET" build "$ROOT/tools/Sts2Oracle" -c Release -v quiet -nologo -m:1
python3 "$ROOT/tools/coverage/map_source_inventory.py" --check "$ROOT/docs/audits/map-source-inventory.json"

step "L2: new-run setup and act maps vs the game ($SEEDS seeds)"
DOTNET="$DOTNET" python3 "$ROOT/tests/l2/compare_newrun.py" --seeds "$SEEDS"

step "Trial DoubleDown: production confirmation/cancel callbacks, forced death and Choice clone/restore"
TRIAL_RESULTS="$(mktemp -d "$ROOT/artifacts/trial-gate-XXXXXX")"
DOTNET="$DOTNET" python3 "$ROOT/tests/api/trial_confirmation.py" --out "$TRIAL_RESULTS/results"

step "L3: Ironclad first-floor combats vs the game ($SEEDS seeds x 2 policies x A0/A10)"
L3_SEEDS="$(python3 -c "print(','.join('L3CHK%04d' % i for i in range($SEEDS)))")"
for asc in 0 10; do
  DOTNET="$DOTNET" python3 "$ROOT/tests/l3/l3_combat.py" --seeds "$L3_SEEDS" --policy-seeds 1,2 --ascension "$asc" --require-complete
done

step "L3: newly implemented non-hero relic combat hooks"
DOTNET="$DOTNET" python3 "$ROOT/tests/l3/l3_combat.py" --seeds SEED0002 --policy-seeds 1 --characters IRONCLAD --encounter CULTISTS_NORMAL --relics BLESSED_ANTLER,CROSSBOW,DAUGHTER_OF_THE_WIND,DIVINE_DESTINY,INFUSED_CORE,IRON_CLUB,PHYLACTERY_UNBOUND,POLLINOUS_CORE,RING_OF_THE_DRAKE,ROYAL_POISON,SAI,SPIKED_GAUNTLETS,VELVET_CHOKER,BRILLIANT_SCARF --max-actions 300 --require-complete

step "L3: Untouchable source regression (base/upgraded bounded probes)"
DOTNET="$DOTNET" python3 "$ROOT/tests/l3/l3_combat.py" --seeds AUUNTOUCH --characters SILENT --probe-cards UNTOUCHABLE,UNTOUCHABLE+1 --probe-helper '' --starting-energy 99 --require-probe-play UNTOUCHABLE --require-complete

step "L4: Ironclad runs to the act 1 boss vs the game ($SEEDS seeds x 2 policies, with and without Neow)"
L4_SEEDS="$(python3 -c "print(','.join('L4CHK%04d' % i for i in range($SEEDS)))")"
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds "$L4_SEEDS" --policy-seeds 1,2 --no-neow --require-complete
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds "$L4_SEEDS" --policy-seeds 1,2 --require-complete

step "L4 coverage-v2: Silent plays, potion actions, all subset sizes and reward reroll/skip"
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds POLICYV2CHOICE01,POLICYV2CHOICE02 --characters SILENT --policy-seeds 1,2 --no-neow --hp 10000 --relics DRIFTWOOD,GAMBLING_CHIP --stop before-act1-boss --verify-snapshots --require-complete --policy-version coverage-v2

step "L4 coverage-v3: native reward availability after reroll"
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds POLICYV3CHOICE01,POLICYV3CHOICE02 --characters SILENT --policy-seeds 1,2 --no-neow --hp 10000 --relics DRIFTWOOD,GAMBLING_CHIP --stop before-act1-boss --verify-snapshots --require-complete --policy-version coverage-v3

# Fixed regression routes keep their historical policy trajectories.
step "L4: native Cheese/BingBong and nested rest reward continuations"
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/run_continuation_regressions.py"

step "L4 focused relic runs: Sand Castle, Wongo ticket, Pumpkin Candle Kindle, and Regal Pillow"
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0002 --policy-seeds 1 --no-neow --hp 10000 --relics SAND_CASTLE --stop after-act1 --verify-snapshots --require-complete --policy-version legacy-v1
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0002 --policy-seeds 1 --no-neow --hp 10000 --relics WONGOS_MYSTERY_TICKET --stop after-act1 --verify-snapshots --require-complete --policy-version legacy-v1
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0002 --policy-seeds 1 --no-neow --hp 10000 --relics PUMPKIN_CANDLE --stop after-act1 --verify-snapshots --require-complete --policy-version legacy-v1
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0008 --policy-seeds 1 --no-neow --hp 10000 --stop after-act1 --verify-snapshots --require-complete --policy-version legacy-v1
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0002 --policy-seeds 1 --no-neow --hp 1000 --relics BLACK_STAR,BLESSED_ANTLER,BLOOD_SOAKED_ROSE,GIRYA,GLITTER,LORDS_PARASOL,PANDORAS_BOX,PHYLACTERY_UNBOUND,POLLINOUS_CORE,SHOVEL,WAR_HAMMER --stop after-act1 --verify-snapshots --require-complete --policy-version legacy-v1

step "L4 Ironclad continuation through Act 2, with per-decision snapshot restore"
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0002 --policy-seeds 1,2 --no-neow --hp 10000 --stop after-act2 --verify-snapshots --require-complete --policy-version legacy-v1
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0004 --policy-seeds 1 --no-neow --hp 10000 --stop after-act2 --verify-snapshots --require-complete --policy-version legacy-v1
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0003 --policy-seeds 1,2 --no-neow --hp 10000 --stop after-act2 --verify-snapshots --require-complete --policy-version legacy-v1

step "L4 Ironclad continuation through the final victory, with per-decision snapshot restore"
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0004 --policy-seeds 1 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete --policy-version legacy-v1
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0002,SEED0003 --policy-seeds 1,2 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete --policy-version legacy-v1
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0005,SEED0006 --policy-seeds 1 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete --policy-version legacy-v1
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0007 --policy-seeds 1 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete --policy-version legacy-v1

step "L4 Pael's Wing: two selected sacrifices and their ordinary relic grant"
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/l4_run.py" --seeds SEED0002 --policy-seeds 1 --no-neow --hp 10000 --relics PAELS_WING --force-sacrifice 2 --stop after-act1 --verify-snapshots --require-complete --policy-version legacy-v1

step "L4 normal Silent A10: Pandora inspection and Winged Boots boss travel adapters"
DOTNET="$DOTNET" python3 "$ROOT/tests/l4/check_nosl_oracle_adapters.py" --out "$ROOT/artifacts/check-nosl-oracle-adapters"

printf '\nall checks passed\n'
