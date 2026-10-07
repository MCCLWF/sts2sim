#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct MasterPlannerPower : PowerModel {
    SIM_MODEL(MasterPlannerPower, PowerModel)

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner().Player() || play->Card.Type() != CardType::Skill) return;
        CardCmd::ApplyKeyword(play->Card, {CardKeyword::Sly});
    }
};

}  // namespace sts2
