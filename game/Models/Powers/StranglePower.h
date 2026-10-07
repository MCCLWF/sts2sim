#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct StranglePower : PowerModel {
    SIM_MODEL(StranglePower, PowerModel)

    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || !Applier().Player() || play->Card.Owner() != Applier().Player()) return;
        CombatState().CaptureCardPlayPowerAmount(*this, play->Card, Amount());
    }

    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play) return;
        int32_t damage = 0;
        if (CombatState().TakeCardPlayPowerAmount(*this, play->Card, damage) && damage > 0)
            CreatureCmd::Damage(ctx, Owner(), Dec(damage), ValueProp::Unblockable | ValueProp::Unpowered,
                                Creature{}, CardModel{}, nullptr);
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
