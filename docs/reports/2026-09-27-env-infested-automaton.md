# Infested Automaton differential result

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Events/InfestedAutomaton.cs`.

Both event options now use the ordinary card reward creation path. Study filters to Power cards. Touch Core uses a serializable, generic card pool predicate for zero canonical energy cost and non-X cost, with the source `NoCardPoolModifications` flag. The generic `CardCreationOptions::GetPossibleCards` now evaluates rarity and type on canonical card instances, matching the original getter semantics.

The original Ironclad `SEED0004` policy 1 Act 2 trace chooses Study. It advanced from 317/737 to 465/737 decisions, without differences and with per-decision snapshot restore. This validates Study on that route, including its RNG continuation. Touch Core still needs a targeted original differential trace. The next stop was Colorful Philosophers.
