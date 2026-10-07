#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct CollisionCourse : CardModel {
    SIM_MODEL(CollisionCourse, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardPileCmd::AddGeneratedCardToCombat(CombatState().CreateCard(CardId::DEBRIS, Owner()), PileType::Hand, Owner());
    }
};
}  // namespace sts2
