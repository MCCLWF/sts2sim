// Models/Cards/Acrobatics.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Acrobatics : CardModel {
    SIM_MODEL(Acrobatics, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        CardList selected = CardSelectCmd::FromHandForDiscard(ctx, Owner(), CardSelectorPrefs(1), nullptr, *this);
        if (!selected.empty()) CardCmd::Discard(ctx, selected[0]);
    }
};
}
