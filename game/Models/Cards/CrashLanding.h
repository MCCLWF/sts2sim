#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct CrashLanding : CardModel {
    SIM_MODEL(CrashLanding, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
        int32_t count = std::max(0, CardPile::MaxCardsInHand - CardPile::Get(PileType::Hand, Owner()).Count());
        CardList debris;
        for (int32_t i = 0; i < count; ++i) debris.push_back(CombatState().CreateCard(CardId::DEBRIS, Owner()));
        CardPileCmd::AddGeneratedCardsToCombat(debris, PileType::Hand, Owner());
    }
};
}  // namespace sts2
