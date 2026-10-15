# Pael's Wing selected alternative differential

Date: 2026-09-27. Fixed original source: v0.111.0 `PaelsWing.cs` and `CardRewardAlternative.cs`.

The full-run oracle gained an opt-in `--force-sacrifice N` test policy. It selects `SACRIFICE` while available for the first N card rewards, leaving the default random policy unchanged. The L4 runner verifies that N selections were actually recorded, so a run with no Pael's Wing choice cannot pass accidentally.

`python3 tests/l4/l4_run.py --seeds SEED0002 --policy-seeds 1 --no-neow --hp 10000 --relics PAELS_WING --force-sacrifice 2 --stop after-act1 --verify-snapshots --require-complete --verbose` passed: one complete original run, 219/219 decisions identical, zero Unported and zero differences. Per-decision snapshot restore was enabled. This path tests the first and second sacrifice, the relic's visible counter, reward completion, and the second sacrifice's ordinary relic grant. It is included in `scripts/check.sh`. No CombatSolver code was changed.
