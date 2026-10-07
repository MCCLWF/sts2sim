# CombatSolver source provenance

This directory translates search policies from
[Torch1230/CombatSolver](https://github.com/Torch1230/CombatSolver), version 0.47.3,
commit `d7b46203593beb51f792628a2587b04f0850d1c2`. The MIT license and copyright
notices are preserved in `LICENSE`. The upstream repository is pinned by the
`external/CombatSolver` submodule. `sts2-ai` is a comparison project and is not
the source of this implementation.

| Native file | Upstream source / translated scope |
| --- | --- |
| `weights.generated.h` | `src/Search/SolverWeights.cs`: all constant declarations; runtime processor policy is not yet ported |
| `policy.cpp` | `ActEndingBossPolicy.cs`: pure value policy and relic heal profile; live CombatState/RunState resolution pending |
| `policy.cpp` | `PotionUsePolicy.cs`: pure arithmetic, replacement credit, ambergris restriction, eligibility |
| `policy.cpp` | `RouteQuality.cs`, `RouteQualityPolicy.cs`: value record and comparisons |
| `policy.cpp` | `SolverInterimResultOrdering.cs`: complete victory, resource trade, interim/display ordering; primary wrapper consumed via RouteQuality |
| `policy.cpp` | `TheftEncounterStrategy.cs`: recovery comparison and satisfaction; encounter and stolen-resource mapping pending |
| `policy.cpp` | `CombatBeamSolver.Transpositions.cs`: six-dimensional label frontier, inline singleton, original admission order |
| `policy.h`, `policy.cpp` | `SolverSearchProfile.cs`: default pure flags/limits; experimental model and perturbation types pending; `SearchBudgetLedger.cs`: pure window only |
| `solver.cpp` | Native integration of the upstream scoring weights, route comparison, potion counterfactual, six-dimensional transposition policy, and potion valuation registry; simplified vulnerability/focus features inspired by `CombatBeamSolver.StateEvaluation.cs` |

The Env adapter, turn Beam, actual EndTurn forecast, focused portfolio, and
adaptive widening are native integration code. They intentionally use a
simpler search organization. Env logical hashes govern exact pruning and
selected-route replay. Heuristic diversity buckets and action symmetry are
approximate search pruning, not replacements for the simulation state key.

Outcome comparison on fixed roots is required before making quality claims.
The upstream experimental ranking models, complete setup/growth/resource
valuation, stolen-resource mapping, continuation cache, and parallel scheduler
are not implemented. Death-save HP and gross loss for effects that damage and
heal within one action are approximate. These limits do not alter game or Env
execution. See `README.md` and the execution report for the tested scope.
