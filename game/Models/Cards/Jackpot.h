#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Jackpot : CardModel {
    SIM_MODEL(Jackpot, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Jackpot target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        CardList cards = CardFactory::GetForCombatZeroCost(Owner(), sts2::data(Owner().Character()).card_pool,
                                                          DynamicVars().Cards().IntValue(),
                                                          Owner().RunState().Rng().CombatCardGeneration());
        for (CardModel card : cards) {
            if (CurrentUpgradeLevel() > 0) CardCmd::Upgrade(card);
            CardPileCmd::AddGeneratedCardToCombat(card, PileType::Hand, Owner());
        }
    }
};

}  // namespace sts2
