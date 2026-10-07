// Models/Cards/DaggerThrow.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct DaggerThrow : CardModel {
    SIM_MODEL(DaggerThrow, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardPileCmd::Draw(ctx, Dec(1), Owner());
        CardList selected = CardSelectCmd::FromHandForDiscard(ctx, Owner(), CardSelectorPrefs(1), nullptr, *this);
        if (!selected.empty()) CardCmd::Discard(ctx, selected[0]);
    }
};
}
