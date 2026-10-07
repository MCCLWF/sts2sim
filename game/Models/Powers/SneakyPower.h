#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct SneakyPower : PowerModel {
    SIM_MODEL(SneakyPower, PowerModel)

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner().Creature() == Owner() || play->Card.Type() != CardType::Attack) return;
        CreatureCmd::GainBlock(Owner(), Dec(Amount()), ValueProp::Unpowered, nullptr, true);
    }
};

}  // namespace sts2
