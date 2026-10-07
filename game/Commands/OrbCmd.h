// Commands/OrbCmd.cs and Entities/Orbs/OrbQueue.cs
#pragma once
#include "game/All.h"

namespace sts2::OrbCmd {

void AddSlots(Player player, int32_t amount);
void RemoveSlots(Player player, int32_t amount);
void Channel(OrbId id, PlayerChoiceContext& choiceContext, Player player);
void EvokeNext(PlayerChoiceContext& choiceContext, Player player, bool dequeue = true);
void EvokeLast(PlayerChoiceContext& choiceContext, Player player, bool dequeue = true);
void Passive(PlayerChoiceContext& choiceContext, OrbModel orb, Creature target, bool countAffectedByHooks = false);
void BeforeTurnEnd(PlayerChoiceContext& choiceContext, Player player);
void AfterTurnStart(PlayerChoiceContext& choiceContext, Player player);

}  // namespace sts2::OrbCmd
