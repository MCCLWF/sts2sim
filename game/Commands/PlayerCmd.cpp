// Commands/PlayerCmd.cs
#include "game/Commands/PlayerCmd.h"

#include <algorithm>

#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Hooks/Hook.h"

namespace sts2::PlayerCmd {

void GainEnergy(Dec amount, Player player) {
    if (amount <= Dec(0) || CombatManager(&player.sim()).IsEnding()) return;
    CombatState combatState = player.Creature().CombatState();
    ModelList modifiers;
    Dec finalAmount = Hook::ModifyEnergyGain(combatState, player, amount, modifiers);
    Hook::AfterModifyingEnergyGain(combatState, modifiers);
    if (finalAmount > Dec(0)) player.PlayerCombatState().GainEnergy(finalAmount);
}

void LoseEnergy(Dec amount, Player player) {
    if (amount <= Dec(0)) return;
    if (CombatManager(&player.sim()).IsEnding()) return;
    player.PlayerCombatState().LoseEnergy(amount);
}

void SetEnergy(Dec amount, Player player) {
    if (CombatManager(&player.sim()).IsEnding()) return;
    int32_t energy = player.PlayerCombatState().Energy();
    if (Dec(energy) < amount) {
        GainEnergy(amount - Dec(energy), player);
    } else if (Dec(energy) > amount) {
        LoseEnergy(Dec(energy) - amount, player);
    }
}

void GainStars(Dec amount, Player player) {
    if (CombatManager(&player.sim()).IsEnding()) return;
    if (!Hook::ShouldGainStars(player.Creature().CombatState(), amount, player)) return;
    player.PlayerCombatState().GainStars(amount);
    Hook::AfterStarsGained(player.Creature().CombatState(), amount.to_int(), player);
}

void LoseStars(Dec amount, Player player) {
    if (CombatManager(&player.sim()).IsEnding()) return;
    player.PlayerCombatState().LoseStars(amount);
}

void SetStars(Dec amount, Player player) {
    if (CombatManager(&player.sim()).IsEnding()) return;
    int32_t stars = player.PlayerCombatState().Stars();
    if (Dec(stars) < amount) {
        GainStars(amount - Dec(stars), player);
    } else if (Dec(stars) > amount) {
        LoseStars(Dec(stars) - amount, player);
    }
}

void GainGold(Dec amount, Player player, bool wasStolenBack) {
    RunState runState = player.RunState();
    ModelList modifiers;
    amount = Hook::ModifyGoldGained(runState, player.Creature().CombatState(), amount, player, modifiers);
    Hook::AfterModifyingGoldGained(runState, player.Creature().CombatState(), modifiers, player, amount);
    if (!(amount > Dec(0))) return;
    player.SetGold(player.Gold() + amount.to_int());
    Hook::AfterGoldGained(runState, player);
}

void LoseGold(Dec amount, Player player, GoldLossType goldLossType) {
    player.SetGold(std::max(0, player.Gold() - amount.to_int()));
}

void SetGold(Dec amount, Player player) {
    int32_t gold = player.Gold();
    if (Dec(gold) < amount) {
        GainGold(amount - Dec(gold), player);
    } else if (Dec(gold) > amount) {
        LoseGold(Dec(gold) - amount, player);
    }
}

void GainMaxPotionCount(int32_t amount, Player player) { player.AddToMaxPotionCount(amount); }
void LoseMaxPotionCount(int32_t amount, Player player) { player.SubtractFromMaxPotionCount(amount); }

void AddPet(Creature pet, Player player) {
    if (!pet.CombatState()) throw GameError("Pet must already be added to a combat state.");
    player.PlayerCombatState().AddPetInternal(pet);
    CreatureCmd::Add(pet);
}

void EndTurn(Player player, bool canBackOut) {
    CombatData& c = player.sim().g->combat;
    if (c.player_ready_to_end_turn) return;  // IsPlayerReadyToEndTurn
    c.player_actions_disabled = true;         // OnEndedTurnLocally
    if (!c.exists) return;                    // SetReadyToEndTurn: no turn state
    c.player_ready_to_end_turn = true;
}

}  // namespace sts2::PlayerCmd
