// Models/Cards/Adrenaline.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Adrenaline : CardModel {
    SIM_MODEL(Adrenaline, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        // The full-screen effect is presentation only.
        PlayerCmd::GainEnergy(Dec(DynamicVars().Energy().IntValue()), Owner());
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};

}  // namespace sts2
