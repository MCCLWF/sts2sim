// Models/Relics/RainbowRing.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct RainbowRing : RelicModel {
    SIM_MODEL(RainbowRing, RelicModel)
    void Reset() const {
        for (int i = 0; i < 4; ++i) extra(i) = 0;
        SetStatus(RelicStatus::Normal);
    }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) Reset();
    }
    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress() || extra(3)) return;
        switch (play->Card.Type()) {
        case CardType::Attack: ++extra(0); break;
        case CardType::Skill: ++extra(1); break;
        case CardType::Power: ++extra(2); break;
        default: break;
        }
        if (extra(0) && extra(1) && extra(2)) {
            Creature creature = Owner().Creature();
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, creature, DynamicVars().Strength().BaseValue(), creature, CardModel{});
            PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, creature, DynamicVars().Dexterity().BaseValue(), creature, CardModel{});
            extra(3) = 1;
            SetStatus(RelicStatus::Active);
        }
    }
    void AfterCombatEnd(CombatRoom) const { Reset(); }
};

}  // namespace sts2
