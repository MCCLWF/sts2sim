#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct RocketPunch : CardModel { SIM_MODEL(RocketPunch, CardModel)
    void AfterCardGeneratedForCombat(CardModel card, Player creator) const {
        if (creator == Owner() && card.Owner() == Owner() && card.Type() == CardType::Status)
            EnergyCost().AddUntilPlayed(-1);
    }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};
}
