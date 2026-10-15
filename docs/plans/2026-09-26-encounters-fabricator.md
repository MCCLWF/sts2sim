# Fabricator Normal encounter

## Goal

Port `FABRICATOR_NORMAL` from game v0.111.0 with its spawn choices, four bot models, and full battlefield behavior.

## Tasks

- [x] Add Fabricator's alive-count branch, attack/spawn moves, and source-matched bot selection RNG.
- [x] Add Guardbot, Noisebot, Stabbot, Zapbot, and HighVoltagePower.
- [x] Add the encounter slots and generated dispatch.
- [x] Compare A0/A10 traces against Sts2Oracle through spawn selection, bot actions, and full-board attacks.
- [x] Update encounter coverage, README, roadmap, aggregate report, and this report; run the quick gate.

## Acceptance

- Defensive and aggressive bot selections use the source option order, exclude the previous bot of the same family, and occupy the next free bot slot.
- Bot moves, MinionPower, Dazed generation, and HighVoltagePower match the oracle.
- `scripts/check.sh --quick` passes.

## Files

- `game/Models/Monsters/Fabricator.h`
- `game/Models/Monsters/{Guardbot,Noisebot,Stabbot,Zapbot}.h`
- `game/Models/Powers/HighVoltagePower.h`
- `game/Models/Encounters/FabricatorNormal.h`
- Generated dispatch, coverage docs, and batch report.
