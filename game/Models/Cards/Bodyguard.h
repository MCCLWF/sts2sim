#pragma once

#include "game/Commands/OstyCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Bodyguard : CardModel {
    SIM_MODEL(Bodyguard, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        OstyCmd::Summon(ctx, Owner(), DynamicVars().Summon().BaseValue(), *this);
    }
};

}  // namespace sts2
