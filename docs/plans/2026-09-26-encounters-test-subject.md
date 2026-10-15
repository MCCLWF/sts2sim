# Test Subject Boss encounter

Port the v0.111.0 three-form Test Subject boss using the existing synchronous combat model. Preserve the death/respawn turn boundary, phase powers, move cycles, and encounter-only status generation. Validate against the game logic in A0/A10, including scripted phase transitions and long turn loops.

- [x] Encounter and Test Subject monster with dynamic HP and all moves.
- [x] Adaptable, Enrage, Painful Stabs, and Nemesis powers.
- [x] Dispatch, build, and A0/A10 L3 probes for all three forms.
- [x] Quick gate, aggregate report, and commit.

Scope: Ironclad and encounter Status cards only; no other playable-character cards.
