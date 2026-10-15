# Metamorphosis and generic combat-long free cost

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Cards/Metamorphosis.cs` and `Models/CardModel.cs`.

The existing C++ card API could set energy cost for the rest of combat but had no public equivalent for the source's star cost setter. Added `CardModel::SetToFreeThisCombat` to append the source's combat-long zero star temporary cost and call the existing combat-long zero energy modifier. The existing Touch of Insanity potion now calls that shared operation instead of changing energy alone. Its selection filter has not been changed or validated here.

Added Metamorphosis' Attack-only `CardFactory::GetForCombat` call using `CombatCardGeneration`, sequential free-cost and random draw insertion for each generated card, and its +2 Cards upgrade. The supported full-unlock profile allows the current character pool enumeration to match the original unlocked pool; epoch-specific locked profiles remain unsupported by the wider Env.

`python3 tools/gen/gen_dispatch.py` registered both play and upgrade hooks, and `cmake --build build -j "$(nproc)"` passed. Fresh original differential: `python3 tests/l4/l4_run.py --seeds SEED0003 --policy-seeds 2 --no-neow --hp 10000 --stop after-act2 --verify-snapshots --verbose` passed all **595/595 decisions** through the Act 2 boundary, after an earlier 245-decision prefix stopped at Metamorphosis. The original `CardCmd.PreviewCardPileAdd` is presentation-only and has no headless state effect. CombatSolver was not changed.
