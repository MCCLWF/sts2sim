# Thieving Hopper Weak encounter

Port the v0.111.0 solo encounter, including deck-card theft priorities and RNG, Swipe's stolen card lifetime and return reward, Flutter damage reduction and stun, and escape countdown/cycle. Other playable characters' cards remain excluded.

- [x] Encounter, monster and EscapeArtist/Flutter/Swipe powers.
- [x] SpecialCard reward construction, population and direct selection.
- [x] Generate dispatch and compile.
- [x] A0/A10 oracle traces plus theft-priority, Flutter depletion, death return and escape probes.
- [x] Quick gate, coverage documentation and commit.

Acceptance: the same theft candidate, card removal and RNG draw as the game; five powered unblocked hits remove Flutter and replace the current move with STUNNED, followed by the source follow-up move; death returns the original deck card via a directly selected SpecialCard reward, while escape keeps it stolen.
