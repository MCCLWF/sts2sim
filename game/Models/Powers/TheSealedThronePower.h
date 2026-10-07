#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct TheSealedThronePower : PowerModel {
    SIM_MODEL(TheSealedThronePower, PowerModel)
    void BeforeCardPlayed(const CardPlay* play) const {
        if (play && play->Card.Owner() == Owner().Player()) PlayerCmd::GainStars(Dec(Amount()), Owner().Player());
    }
};
}  // namespace sts2
