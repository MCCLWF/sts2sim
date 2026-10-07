#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

// The original keeps a dictionary from active CardPlay card identity to the
// amount observed before that play. CombatState snapshots the same keyed data
// so any number of nested plays within the actual card/power pools is retained.
struct AfterimagePower : PowerModel {
    SIM_MODEL(AfterimagePower, PowerModel)

    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.Owner().Creature() != Owner()) return;
        CombatState().CaptureCardPlayPowerAmount(*this, play->Card, Amount());
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner().Creature() != Owner()) return;
        int32_t amount = 0;
        if (CombatState().TakeCardPlayPowerAmount(*this, play->Card, amount) && amount > 0)
            CreatureCmd::GainBlock(Owner(), Dec(amount), ValueProp::Unpowered, nullptr, true);
    }
};

}  // namespace sts2
