# Common potions batch 5

- Branch: `feat/common-potions-5`
- Scope: implement Cure All, Lucky Tonic, Stable Serum, Powdered Demise, and Duplicator together with their direct combat powers.
- Reuse existing energy, draw, HP-loss, card-play-count, and turn-end hook paths.
- Acceptance: all five potions and four supporting powers are marked usable, focused tests cover immediate effects and expiration/trigger behavior, and full `scripts/check.sh` remains green.
