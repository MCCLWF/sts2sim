# Pael's Wing card reward alternative status

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Relics/PaelsWing.cs`, `CardRewardAlternative.cs`, and `CardReward.cs`.

The generic Env now carries Pael's Wing's saved sacrifice counter and ShowCounter/DisplayAmount. The ordinary card reward choice generator appends `SACRIFICE` after Skip/Reroll while preserving the original two-alternative capacity limit. Selecting it completes the reward, increments the relic's counter, and obtains the next ordinary relic on every second sacrifice. Other unsupported reward-alternative hooks still raise Unported. The generator records this manually implemented guarded hook when computing usable model coverage.

On the original Ironclad `SEED0004` policy 2 Act 2 trace, Pael's Wing is chosen and five later card reward choices show `['Skip','SACRIFICE']`. Replay with per-decision snapshot restore advances from the old 250/771 stop to 466/771 decisions, with no difference on that prefix; it next stops at an unported Iteration card in the combat root. This policy never selects Sacrifice.

An opt-in oracle policy now forces the first two available `SACRIFICE` selections and the L4 runner rejects a trace unless both choices actually occurred. `SEED0002` policy 1, A0, no Neow, 10000 HP, starting with `PAELS_WING`, matches **219/219 decisions through the Act 1 boundary**, including the saved counter and ordinary relic grant after the second sacrifice. Command: `python3 tests/l4/l4_run.py --seeds SEED0002 --policy-seeds 1 --no-neow --hp 10000 --relics PAELS_WING --force-sacrifice 2 --stop after-act1 --verify-snapshots --require-complete --verbose`. The targeted path is now in `scripts/check.sh`; it validates two selections, not every reward modifier combination.
