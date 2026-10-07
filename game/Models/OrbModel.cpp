// Models/OrbModel.cs
#include "game/Models/OrbModel.h"

#include "game/All.h"
#include "game/Hooks/Hook.h"

namespace sts2 {

Player OrbModel::Owner() const { return st().owner ? Player(sim_, st().owner) : Player{}; }

sts2::CombatState OrbModel::CombatState() const { return Owner().Creature().CombatState(); }

Dec OrbModel::ModifyOrbValue(Dec value) const { return Hook::ModifyOrbValue(CombatState(), *this, value); }

void OrbModel::TriggerPassive(PlayerChoiceContext& choiceContext, Creature target) const {
    ModelList modifiers;
    int32_t count = Hook::ModifyOrbPassiveTriggerCount(CombatState(), *this, 1, modifiers);
    Hook::AfterModifyingOrbPassiveTriggerCount(CombatState(), *this, modifiers);
    for (int32_t i = 0; i < count; ++i) Passive(choiceContext, target);
}

}  // namespace sts2
