# Generic Env: Metamorphosis and combat-long free cost

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Cards/Metamorphosis.cs` and `CardModel.SetToFreeThisCombat`.

Implement the generic `CardModel::SetToFreeThisCombat` operation for both energy and star cost. Use it for Metamorphosis and the existing Touch of Insanity potion. Port Metamorphosis' unlocked Attack pool generation with replacement, exact RNG streams, sequential random draw insertion, and +2 Cards upgrade. Generate dispatch, build and differential-test the original Ironclad path. Keep locked profiles unsupported until the underlying Env supports their exact epoch unlocks. CombatSolver remains frozen.
