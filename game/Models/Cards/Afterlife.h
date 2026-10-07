#pragma once

#include "game/Commands/OstyCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Afterlife : CardModel {
    SIM_MODEL(Afterlife, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        OstyCmd::Summon(ctx, Owner(), DynamicVars().Summon().BaseValue(), *this);
    }
};

}  // namespace sts2
