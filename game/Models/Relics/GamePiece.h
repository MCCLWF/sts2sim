// Models/Relics/GamePiece.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct GamePiece : RelicModel {
    SIM_MODEL(GamePiece, RelicModel)

    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress() ||
            play->Card.Type() != CardType::Power) return;
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};

}  // namespace sts2
