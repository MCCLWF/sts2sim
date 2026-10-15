# Infested Automaton and card pool predicates

Date: 2026-09-27. Fixed game source: v0.111.0 `Models/Events/InfestedAutomaton.cs`, `Runs/CardCreationOptions.cs`, `Factories/CardFactory.cs`.

The Ironclad `SEED0004` policy 1 Act 2 trace stops at this event after 317/737 decisions. Add its two original options through ordinary reward card creation: Study filters canonical cards to Power; Touch Core filters cards to non-X, zero canonical energy cost and disables card pool modifications. The filter is an owned, serializable generic CardCreationOptions predicate, used by the ordinary CardFactory path; no event-specific random draw or result list is embedded in the Env. Rebuild and replay the original trace with per-decision snapshot restoration. Report subsequent Unported content explicitly.
