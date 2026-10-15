# SandCastle AfterObtained port

Date: 2026-09-27.

## Preconditions

- Generic Env work is active on the Ironclad Orobas run; its current plan identifies Orobas pool 2 as containing SandCastle.
- The v0.111.0 game source was inspected in `Models/Relics/SandCastle.cs`, `Extensions/ListExtensions.cs`, `Random/Rng.cs`, `Commands/CardCmd.cs`, and `Models/CardModel.cs`.
- Existing C++ `CardPileCmd::StableShuffle` sorts using `CardModel::CompareTo` then calls the same descending Fisher–Yates shuffle. Existing `CardCmd::Upgrade` upgrades and finalizes each eligible card.

## Change

Added `game/Models/Relics/SandCastle.h`. The relic filters the deck in its existing order to non-null upgradable cards, stable-shuffles with the run's Niche RNG, takes up to the `Cards` dynamic variable (canonical 6), and upgrades the selected cards. The original forced preview-column setting is omitted because it only affects the UI.

## Verification

- Source semantics and matching shared primitives: inspected; no shared primitive change was needed.
- Integration: `tools/gen/gen_dispatch.py` and Release CMake build passed. Targeted original Oracle comparison with `l4_run.py --seeds SEED0002 --policy-seeds 1 --no-neow --hp 10000 --relics SAND_CASTLE --stop after-act1 --verify-snapshots --require-complete --verbose` passed **1/1 run, 172/172 decisions**, zero mismatch/Unported; `/tmp/mcc-env-sand-castle-l4.log`.

## Limits

The targeted one-seed Ironclad path verifies the pickup effect and subsequent run state, but it does not cover every deck composition or later acquisition point. Complete Orobas option branching and all characters remain unverified.
