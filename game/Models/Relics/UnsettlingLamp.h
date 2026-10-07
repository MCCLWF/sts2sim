#pragma once

#include "game/Entities/Creatures/Creature.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/PowerModel.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct UnsettlingLamp : RelicModel {
    SIM_MODEL(UnsettlingLamp, RelicModel)

    void BeforeCombatStart() const {
        extra(0) = 0;  // triggering card
        extra(1) = 0;  // first doubled power
        extra(2) = 0;  // finished triggering
        SetStatus(RelicStatus::Active);
    }

    void BeforePowerAmountChanged(PowerModel power, Dec amount, Creature target, Creature applier,
                                  CardModel cardSource) const {
        if (extra(0) != 0 || extra(2) != 0 || !cardSource) return;
        Creature ownerCreature = Owner().Creature();
        if (applier != ownerCreature || target.Side() == ownerCreature.Side()) return;
        if (!power.IsVisible() || power.GetTypeForAmount(amount) != PowerType::Debuff) return;
        if (target.HasPower(PowerId::ARTIFACT_POWER)) return;

        extra(0) = cardSource.Index();
        extra(1) = power.Index();
    }

    Dec ModifyPowerAmountGivenMultiplicative(PowerModel power, Creature, Dec amount, Creature, CardModel cardSource) const {
        if (extra(0) == 0 || cardSource.Index() != extra(0) || extra(2) != 0) return Dec(1);
        if (extra(1) != 0) {
            PowerModel doubledPower(&sim(), static_cast<PowerRef>(extra(1)));
            PowerId internallyApplied = doubledPower.InternallyAppliedPowerId();
            if (internallyApplied != PowerId::NONE && power.PowerId_() == internallyApplied) return Dec(1);
        }
        return power.GetTypeForAmount(amount) == PowerType::Debuff ? Dec(2) : Dec(1);
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Index() != extra(0) || extra(2) != 0) return;
        extra(2) = 1;
        SetStatus(RelicStatus::Normal);
        Flash();
    }

    void AfterCombatEnd(CombatRoom) const {
        extra(0) = 0;
        extra(1) = 0;
        extra(2) = 0;
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
