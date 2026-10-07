// Commands/PlayerCmd.cs. Sound effects and the map point history (run
// statistics) are dropped.
#pragma once

#include "game/All.h"

namespace sts2::PlayerCmd {

void GainEnergy(Dec amount, Player player);
void LoseEnergy(Dec amount, Player player);
void SetEnergy(Dec amount, Player player);
void GainStars(Dec amount, Player player);
void LoseStars(Dec amount, Player player);
void SetStars(Dec amount, Player player);
void GainGold(Dec amount, Player player, bool wasStolenBack = false);
void LoseGold(Dec amount, Player player, GoldLossType goldLossType = GoldLossType::Lost);
void SetGold(Dec amount, Player player);
void GainMaxPotionCount(int32_t amount, Player player);
void LoseMaxPotionCount(int32_t amount, Player player);
void AddPet(Creature pet, Player player);
// SetReadyToEndTurn. The end-turn signal is handled by whoever drives the
// combat (CombatManager: StartCombat / the player's action entry points).
void EndTurn(Player player, bool canBackOut);

}  // namespace sts2::PlayerCmd
