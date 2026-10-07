# Combat prediction boundary differential checks

This independent .NET tool runs `sts2.dll` with **TestMode=false** and compares
selected semantic fields of the derived Mod's Native prediction branches with
`EnvCombatPredictionSession` branches from the same live root and logical action
sequence. It calls the pinned upstream `CombatBeamSolver.Replay` path, including
its card/potion completion, round transition and forced turn end paths. It does
not run `Solve`, measure search speed or establish installed game acceptance.

## Prerequisites and commands

Build the C++ shared library and derived Mod first. The derived Mod build uses
the pinned CombatSolver source and its explicit local overlays; the original
submodule is unchanged. The repository's `tools/Directory.Build.props` supplies
game paths, with `STS2_GAME_DIR` and `STS2_SRC` overrides. `DOTNET` below may name
the local .NET 9 executable.

```bash
cmake --build build -j2
"${DOTNET:-dotnet}" build tools/CombatSolverEnv -c Release -p:CopyModOnBuild=false
"${DOTNET:-dotnet}" build tools/EnvCombatPredictionChecks -c Release
"${DOTNET:-dotnet}" tools/EnvCombatPredictionChecks/bin/Release/net9.0/EnvCombatPredictionChecks.dll --out /tmp/sts2env-combat-prediction-checks
```

An individual scenario or Native projection can be inspected with:

```bash
"${DOTNET:-dotnet}" tools/EnvCombatPredictionChecks/bin/Release/net9.0/EnvCombatPredictionChecks.dll --scenario extra_turn_poison --out /tmp/sts2env-extra-turn-poison
"${DOTNET:-dotnet}" tools/EnvCombatPredictionChecks/bin/Release/net9.0/EnvCombatPredictionChecks.dll --native-only --scenario strike
```

`--out` writes the assembled live root, Native semantic projection, Env semantic
projection, and complete Env prediction snapshot for each scenario. These raw
artifacts contain privileged hidden state/RNG and are diagnostics, not a player
observation API. `--native-only` prints the Native projection and does not assert
cross-backend agreement.

## Fixtures

All roots are single-player Ironclad, fully unlocked, A0, seed `LIVEROOT001`,
CultistsNormal with both original enemies retained. Native game setup, relic
finalization, combat-start hooks and initial turn setup run before capture.
Manual startup turn flags and a manual turn loop avoid launching the game's
interactive turn loop. The root assembler's seeded session supplies only the
registry/schema and its blank envelope; live gameplay values are captured from
the actual game objects.

The isolated fixtures deliberately adjust HP or add a native model/power through
the game methods. Those edits define the fixture root for **both** predictors.
They are not corrections to either successor. Logical Self targets map to Native
combat ID and the Env API's legal action `target=-1` encoding.

| Scenario | Same logical action sequence | Checks |
|---|---|---|
| `strike` | Strike first cultist; Strike second cultist (2 actions) | Lethal card completion, enemy removal, terminal T1, Burning Blood does not heal prediction HP |
| `enemy` | EndTurn (1) | Forced legal DARK_STRIKE_MOVE kills HP1 player; terminal T1 |
| `phase_one` | EndTurn (1) | Burn in hand kills HP1 player during phase one; terminal T1, Burn remains in Play pile |
| `poison` | EndTurn (1) | Poison2 kills both HP1 enemies at enemy-side start; terminal T1 |
| `enemy_end` | EndTurn (1) | Demise2 kills both HP1 enemies at enemy-side end; terminal T1 without advancing turn |
| `forced_end` | VoidForm (1) | Forced turn end; MercuryHourglass added after initial start kills both HP1 enemies at T2 |
| `fire_potion` | Strike first cultist; FirePotion second cultist (2) | Potion consumption and lethal use completion; terminal T1 |
| `player_poison` | EndTurn (1) | Nonterminal player Poison2, HP3 to HP1 at ordinary T2 player start |
| `extra_turn_poison` | EndTurn (1) | Ambergris1 grants extra turn; player Poison2, HP3 to HP1 at T2; no enemy moves |
| `enemy_end_poison` | EndTurn (1) | Historical discovery name: lethal player Poison2 now triggers at **T2 player start**, not enemy end |

The tool does not initialize the Mod UI or automatic search and does not register
the companion branch verifier. It sets only the Native diagnostic logger and a
headless display provider. Replay uses captured policy with fixed budget,
verification disabled, detailed diagnostics disabled and phase measurements
disabled; no search is executed. Empty display-name dictionaries avoid cosmetic
lookups for upstream mock powers registered in its assembly.

## Exact comparison scope

Each field below is independently projected from the Native **shadow simulator**
and the complete Env snapshot, then compared without normalization of turn,
phase, health, piles, powers or RNG:

- Terminal outcome and stamped player turn; in-progress and pending-loss flags.
- Actual player turn number, energy, stars, player phase, round number and side.
- Every root creature's combat ID, HP, max HP, block, HP display and attachment.
- Effective per-creature power IDs and amounts in order; active enemy roster.
- Ordered Hand, Draw, Discard, Exhaust and Play card identities, upgrade levels,
  enchantment IDs and affliction IDs.
- Potion identities in every actual Native slot; unused fixed Env padding must be
  empty and is not treated as an additional Native slot.
- Nine combat run RNG streams, each with full counter and s0/s1/s2/s3:
  Shuffle, CombatCardGeneration, CombatPotionGeneration, CombatCardSelection,
  CombatEnergyCosts, CombatTargets, CombatOrbs, MonsterAi and Niche.

A passing branch also proves that Env prediction leaves the source hash/document
unchanged, full prediction export/restore retains its hash/document, and Native
Replay leaves the live source `ContinuationStamp` unchanged. Terminal branches
have no legal actions and further prediction fails with no successor and no
source changes. Nonterminal branches must still have legal actions.

This is **not full Native GameState equality**. The Native shadow has no complete
GameState exporter. The check excludes pool identities/capacities, complete event
history, card costs/keywords/dynamic-variable values and private card sidecars,
power private data beyond amount, full relic/orb/monster private state and FSM,
monster personal RNG, noncombat/player random streams, run map/reward state,
choice replay and same-policy search scores/order/budgets. Other tools cover some
of these areas; this tool alone proves none of those excluded requirements.

## Player Poison discrepancy and actual game timing evidence

Before the derived Native lifecycle overlay, `enemy_end_poison` detected the
upstream shadow's early player Poison compensation: Native stamped T1/Enemy,
phase None and piles `[0,5,5,0,0]`; Env stamped T2/Player, phase End and piles
`[5,0,5,0,0]`. The extra-turn fixture also exposed missing Poison in the upstream
extra-turn branch (Native HP3, Env HP1). No expected value or Env timing was
changed to hide these differences. The derived lifecycle overlay moves Native
player Poison to the actual player-start position while retaining the original
search algorithm. Passing results therefore refer to this **derived Native
predictor**, not the unmodified upstream predictor.

The `enemy_end_poison` check additionally records a separate nonlethal actual
game timing trace. After the independent shadow projections are captured, its
live player is explicitly set to HP3; original CombatManager turn methods run.
A scoped, read-only Harmony prefix observes the original
`PoisonPower.AfterSideTurnStart` without replacing or wrapping its Task. Native
HP-change events also only read fields. The resulting
`enemy_end_poison.vanilla-trace.json` proves that the real listener receives its
owner at T2, Player side, phase Start, **after drawing five cards**, and damages
HP3 to HP1. The preceding complete enemy turn leaves HP3 unchanged.

That trace is timing evidence from a distinct nonlethal observation fixture. It
is not a lethal oracle successor and is never substituted for the shadow
projection. Actual lethal `CreatureCmd.Kill` continues into mixed run-ending and
UI code; the tool does not skip that gameplay path as presentation. All VFX/IO
skips come from the documented `Headless.InstallLiveCombatPresentation` scope;
TestMode remains off.

On the 2026-09-27 implementation run, all ten fixtures, source/restore checks and
the separate real timing trace passed. Output explicitly reports
`full_state_comparison=false` and `installed_game_acceptance=false`.
