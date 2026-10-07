#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Overclock : CardModel { SIM_MODEL(Overclock, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        CardPileCmd::AddGeneratedCardToCombat(CombatState().CreateCard(CardId::BURN, Owner()), PileType::Discard, Owner());
    }
};
}
