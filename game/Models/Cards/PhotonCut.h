#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct PhotonCut : CardModel {
    SIM_MODEL(PhotonCut, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        CardList selection = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(DynamicVars()["PutBack"].IntValue()), nullptr, *this);
        if (!selection.empty()) CardPileCmd::Add(selection, PileType::Draw, CardPilePosition::Top);
    }
};
}  // namespace sts2
