# STS2 v0.111.0 shared cards and all potions validation

## Scope and source inventory

- Branch: `feat/shared-cards-all-potions-v0110` (worktree created from `main` at `59b51ec`).
- Source of truth: original `src/Core` and `data/content.json` for v0.111.0.
- Inventory command:

  ```bash
  export STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core'
  python3 tools/gen/shared_content_inventory.py --root "$PWD" \
    --src "$STS2_SRC" --out "$PWD/docs/audits/shared-content-source-inventory.md"
  ```

- Inventory result: **207 models** (**142 cards**, **65 non-mock potions**), **938** exported source override rows, **16** scoped pools, and **207/207** header/include/dispatch registrations. The generated inventory records each override, branch, loop, direct RNG call, generation call, dependency, upgrade, multiplayer constraint, pool candidate list, and source line. No source file or override was missing or unlocated.
- Card scope is 130 single-player models plus 12 explicitly `MultiplayerOnly` models. The latter remain registered and pool-filtered, with their single-player methods intentionally marked as the multiplayer boundary. The nine potion pools include shared, event, deprecated, token, all five character pools, and the three Necrobinder potions.
- `docs/audits/shared-content-source-inventory.md` is the model-by-model checklist; it is registration evidence only and is paired with the behavior results below.
- The source dependency scan finds 63 Power/Affliction/Enchantment symbols in the selected source files: 58 single-player dependencies are implemented and dispatched; the remaining five (`BeaconOfHopePower`, `CoordinatePower`, `CoveredPower`, `KnockdownPower`, `TagTeamPower`) are referenced only by the 12 `MultiplayerOnly` cards and remain an explicit multiplayer boundary.

## Oracle card validation

The card manifest covers every scoped card and its base/upgrade variant. It includes curses, events, quests, statuses, tokens, deprecated cards, generated cards, character pool cards, and the 12 multiplayer-only boundary cards. The ordinary card probes plus the special Mad Science matrix exercised **237/237** base/upgrade variants.

Raw results: `tests/results/shared-content-v0110/cards/l3-final-results.txt` and `tests/results/shared-content-v0110/cards/manifest.json`.

Representative command form (the manifest contains the complete 32-batch command list):

```bash
python3 tests/l3/l3_combat.py \
  --trace tests/results/shared-content-v0110/cards/000-ironclad.jsonl \
  --starting-energy 99 --starting-stars 99 \
  --require-probe-play ALCHEMIZE,ANOINTED,AUTOMATION,BEAT_DOWN
```

Result: `generic=219`, `special=18`, `total=237`, `batches=32`, `failures=0`; every batch reported `0 unported, 0 diverged` and `rc=0`. Mad Science was separately checked in nine configurations (attack: sapping/violence/choking; skill: energized/wisdom/chaos; power: expertise/curious/improvement), each at base and upgrade.

The cross-turn Toric Toughness dependency received an additional focused Oracle probe after the power was completed:

```bash
STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' \
  python3 tests/l3/l3_combat.py --seeds TORIC1101 --characters IRONCLAD \
    --policy-seeds 1 --probe-cards TORIC_TOUGHNESS,TORIC_TOUGHNESS+1 \
    --probe-after-card-turn --probe-helper '' --starting-energy 99 \
    --starting-stars 99 --require-probe-play TORIC_TOUGHNESS \
    --max-actions 100 --cli build/sts2sim_cli \
    --trace-output tests/results/shared-content-v0110/cards/toric-toughness-after-turn.oracle.jsonl
```

Result: `2/2 combats identical, 0 unported, 0 diverged, 6/6 states matched`; this includes the post-card player turn after the block-clear hook. The direct regression in `tests/test_shared_models.cpp` also checks the stored block dynamic var, both block-clear triggers, decrement, and removal.

## Oracle potion validation

All **65/65** non-mock potions were probed by pool, including all five character pools and `POTION_POOL.NECROBINDER_POTION_POOL` (`POTION_OF_DOOM`, `POT_OF_GHOULS`, `BONE_BREW`). Raw commands and traces are recorded in `tests/results/shared-content-v0110/potions/manifest.json`; the summary is `tests/results/shared-content-v0110/potions/l3-final-results.txt`.

Result: 14 pool batches, all `rc=0`, all `0 unported, 0 diverged`; every base use and target/effect path matched the Oracle. The implementation includes the shared potion runtime and the three Necrobinder potion models; the Necrobinder career task consumes these models without duplicating their runtime.

## Full check gate

The exact full command was:

```bash
STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' \
  ./scripts/check.sh 2>&1 | tee tests/results/shared-content-v0110/scripts-check-final.log
```

Result: `100% tests passed, 0 tests failed out of 33`; L2 `200 runs, 0 mismatched`; both L3 blocks `40/40 combats identical, 0 unported, 0 diverged`; L3 shared relic probe `1/1 identical`; L4 act-1 blocks `40/40 identical` and `40/40 identical`; every focused relic, Act 2, final-victory, and Pael sacrifice route reported `0 unported, 0 diverged`; final line: `all checks passed`.

The complete raw output is retained in `tests/results/shared-content-v0110/scripts-check-final.log`.

## Clone, snapshot, and failure contracts

- `./build/test_snapshot` passed: exact scalar restore, owned restore, later-turn restore, choice replay, storage-order preservation, and unported rejection.
- `./build/test_shared_models`, `./build/test_potions`, `./build/test_preview`, and `./build/test_env` passed; output is in `tests/results/shared-content-v0110/direct-tests-final.log`.
- L4 verification with `--verify-snapshots --require-complete` passed in the full check for focused relic routes, Act 2 continuations, final victory, and the Pael sacrifice route.
- `CombatState::CloneCard` and `RunState::CloneCard` reset the simulator-only `last_played_turn` marker. This preserves the source history contract: a clone has no `CardPlayFinishedEntry`, so cloned Bolas/Thrumming Hatchet cards cannot inherit a stale history hit. The full run `9LQ0E8B3Q7` then matched **887/887** decisions after this fix.
- The replay harness rejects Oracle traces with asynchronous task exceptions, reports simulator `Unported` stops as failures under `--require-complete`, and compares decision kind/options, run/combat state, end state, and boundary state.

## New random whole-run round

Fresh candidates were generated after excluding the 200 seeds in rounds 1 and 2 (`tests/seeds/shared-content-v0110-round3-candidates.tsv`). Each candidate was run in an independent Oracle process to avoid unrelated multi-seed presentation-task logging. The accepted set is `tests/seeds/shared-content-v0110-round3.tsv` (**100 unique new seeds; 25 each Ironclad, Silent, Defect, Regent**).

Command form for each independent seed:

```bash
STS2_SRC='/home/wst/Desktop/MCC/Slay the Spire 2/src/Core' \
  python3 tests/l4/l4_run.py --seeds SCV0110R3... --characters IRONCLAD \
    --policy-seeds 1 --hp 10000 --stop victory --require-complete \
    --max-decisions 10000 --cli build/sts2sim_cli \
    --trace-output tests/results/shared-content-v0110/round3/raw/IRONCLAD_...oracle.jsonl
```

Accepted result: **100/100 identical, 0 unported, 0 diverged**; all four characters reached the explicit three-act `victory` endpoint. Per-seed Oracle traces and logs are in `tests/results/shared-content-v0110/round3/{raw,logs}`. The exploratory candidate classification (including rejected differences and unported boundaries) is in `tests/results/shared-content-v0110/round3/round3-candidate-results.tsv`; rejected candidates are not counted as acceptance seeds.

## Presentation and multiplayer boundaries

- Twelve cards marked `MultiplayerOnly` remain source-registered and available to a future multiplayer runtime, but are excluded by the single-player pool filters; their gameplay methods are not claimed by the single-player acceptance.
- Crystal Sphere, Trial portraits, Soul Nexus animations, screen shake/rumble/hit-stop, and NGame presentation callbacks use a headless adapter only. The adapter preserves game actions and RNG draws (Crystal Sphere selection, Trial choice, and shake angle are still source-driven); it suppresses rendering/audio/UI waits so Oracle traces can finish.
- Card/potion hover tips, portraits, card frames, overlays, and localized description hooks are tracked in the inventory as presentation-only. No `IsUsable` or header count is used as behavior proof.

## Remaining boundaries

- Multiplayer-only card effects and multiplayer presentation remain outside this single-player task, as explicitly listed in the inventory.
- Rendering/audio and timing-only effects are not visually asserted in headless validation.
- The 100-seed acceptance round covers the four playable non-Necrobinder routes in this worktree. The parallel Necrobinder career route owns its career-specific card behavior and performs its own five-character/random-run acceptance while consuming the three potion models from this branch.
