// Models/Powers/ThornsPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct ThornsPower : PowerModel {
    SIM_MODEL(ThornsPower, PowerModel)

    void BeforeDamageReceived(PlayerChoiceContext& choiceContext, Creature target, Dec amount, ValueProp props, Creature dealer,
                              CardModel cardSource) const {
        if (target == Owner() && dealer && (IsPoweredAttack(props) || (cardSource && cardSource.CardId_() == CardId::OMNISLICE))) {
            Flash();
            ExecutionSourceScope damageSource(Owner().sim(), ExecutionDamageSourceKind::Thorns, "ThornsPower");
            CreatureCmd::Damage(choiceContext, dealer, Dec(Amount()), ValueProp::Unpowered | ValueProp::SkipHurtAnim, Owner(),
                                CardModel{}, nullptr);
        }
    }
};

}  // namespace sts2
