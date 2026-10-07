// Models/Cards/GunkUp.cs
#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct GunkUp : CardModel { SIM_MODEL(GunkUp, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(DynamicVars().Repeat().IntValue())
            .FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardModel slimed = CombatState().CreateCard(CardId::SLIMED, Owner());
        CardPileCmd::AddGeneratedCardToCombat(slimed, PileType::Discard, Owner());
    }
};
}
