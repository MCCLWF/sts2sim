# Common potions batch 3

- Branch: `feat/common-potions-3`
- Scope: add direct, deterministic common/shared potion effects whose target and value semantics are already represented by the simulator: Explosive Ampoule, Flex Potion, Gigantification Potion, Regen Potion, Shackling Potion, and Speed Potion.
- Include the temporary strength/dexterity and regeneration/gigantification powers needed by those effects.
- Keep card-choice potions (Attack, Power, Skill, Colorless, Bottled Potential, etc.) out of this batch until their choice/replay paths are implemented.
- Acceptance: generated dispatch marks only the six new potions and their supporting powers usable; focused potion/power tests cover apply, expiry or immediate effect; full `scripts/check.sh` remains green.
