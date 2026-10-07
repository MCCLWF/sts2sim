#pragma once
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct AdaptiveStrike : CardModel { SIM_MODEL(AdaptiveStrike, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardModel clone = CreateClone();
        clone.EnergyCost().SetThisCombat(0);
        CardPileCmd::AddGeneratedCardToCombat(clone, PileType::Discard, Owner());
    }
};
}
