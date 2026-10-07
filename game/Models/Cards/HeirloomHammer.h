#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct HeirloomHammer : CardModel {
    SIM_MODEL(HeirloomHammer, CardModel)
    static bool IsColorless(CardModel card) { return card.VisualCardPoolIsColorless(); }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardList selection = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(1), IsColorless, *this);
        if (selection.empty()) return;
        for (int32_t i = 0; i < DynamicVars().Repeat().IntValue(); ++i) {
            CardModel clone = selection[0].CreateClone();
            CardPileCmd::AddGeneratedCardToCombat(clone, PileType::Hand, Owner());
        }
    }
};
}  // namespace sts2
