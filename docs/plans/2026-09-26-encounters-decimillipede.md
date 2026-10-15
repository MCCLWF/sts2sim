# Decimillipede Elite encounter

## Goal

Port `DECIMILLIPEDE_ELITE` from game v0.111.0, including three distinct segment models, the encounter's starter-move RNG, unique even segment HP, and the reattach-on-death loop.

## Tasks

- [x] Add the Decimillipede Elite encounter and segment monster models.
- [x] Add `ReattachPower`, including retained dead segments, one-turn revival protection, and the 25 HP reattach.
- [x] Regenerate dispatch and compile.
- [x] Compare A0/A10 against Sts2Oracle for opening RNG/moves, each move cycle, segment revival, and final death.
- [x] Update coverage and reports; run the quick gate.

## Acceptance

- The encounter draws one starter index and assigns the three distinct opening moves in front/middle/back order.
- Segment HP is even and distinct, preserving the source's wraparound behavior.
- A killed segment remains in combat, skips its next move, heals by 25 after its following reattach move when a sibling remains alive, and cannot be hit while reviving.
- A0/A10 L3 traces match the oracle, including a trace that kills and revives a segment.
- `scripts/check.sh --quick` passes.

The encounter also requires Feed and Hand of Greed to honor the existing death-trigger Fatal power predicate. Both now check it before attacking and use the attack damage results to identify a kill.
