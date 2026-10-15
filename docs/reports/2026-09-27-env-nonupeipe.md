# Nonupeipe third-act ancient

Date: 2026-09-27. Fixed game source: v0.111.0 `Models/Events/Nonupeipe.cs`.

## Change

Added the nine ordinary ancient relic candidates and the conditional Beautiful Bracelet candidate. The event shuffles the full candidate list with its event RNG, then displays the first three through the existing ancient relic option path. The condition counts cards eligible for Swift enchantment using the existing base enchantment eligibility helper.

## Verification

- `python3 tools/gen/gen_dispatch.py`: event registered; generated dispatch rebuilt.
- `cmake --build build -j "$(nproc)"`: passed.
- `python3 tests/l4/l4_run.py --seeds SEED0004 --policy-seeds 1 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete --verbose`: the original trace contains 1123 decisions. Env and per-decision snapshot restore matched through decision 738, then stopped at `Relics.FurCoat.AfterObtained is not ported`. Raw log: `/tmp/mcc-env-seed0004-p1-victory-nonupeipe-l4.log`.

This confirms the displayed options and selected relic for this one path, not every candidate permutation or the Swift condition boundary. Fur Coat remains a third-act blocker. CombatSolver was not changed.
