// Models/PotionModel.cs
#include "game/Models/PotionModel.h"

#include "game/All.h"
#include "game/Combat/CombatManager.h"
#include "game/Hooks/Hook.h"

namespace sts2 {

Player PotionModel::Owner() const { return st().owner ? Player(sim_, st().owner) : Player{}; }

void PotionModel::OnUseWrapper(PlayerChoiceContext& choiceContext, Creature target) const {
    Player player = Owner();
    player.RemoveUsedPotionInternal(*this);
    st().removed = true;
    CombatState combatState = player.Creature().CombatState();
    CombatManager cm(sim_);
    Hook::BeforePotionUsed(player.RunState(), combatState, *this, target);
    cm.BeginCardOrPotionEffect(player);
    try {
        OnUse(choiceContext, target);
    } catch (...) {
        cm.EndCardOrPotionEffect(player);
        throw;
    }
    cm.EndCardOrPotionEffect(player);
    if (!player.Creature().IsDead()) {
        Hook::AfterPotionUsed(player.RunState(), combatState, *this, target);
        cm.CheckForEmptyHand(choiceContext, player);
    }
}

sts2::TargetType PotionModel::TargetType() const {
    if (PotionId_() == PotionId::FOUL_POTION) {
        // FoulPotion.TargetType
        if (!CombatManager(sim_).IsInProgress()) return TargetType::TargetedNoCreature;
        return TargetType::AllEnemies;
    }
    return data().target;
}

}  // namespace sts2
