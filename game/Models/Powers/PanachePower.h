#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct PanachePower : PowerModel {
    SIM_MODEL(PanachePower, PowerModel)

    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner().Player()) return;
        if (extra(0) != 0) {
            DynamicVar left = DynamicVars()["CardsLeft"];
            left.SetBaseValue(left.BaseValue() - Dec(1));
            if (left.IntValue() <= 0) {
                CreatureCmd::Damage(ctx, CombatState().HittableEnemies(), Dec(Amount()), ValueProp::Unpowered, Owner());
                left.SetBaseValue(Dec(5));
            }
        }
        extra(0) = 1;
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) DynamicVars()["CardsLeft"].SetBaseValue(Dec(5));
    }
};

}  // namespace sts2
