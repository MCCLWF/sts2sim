#pragma once

#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct CosmicIndifference : CardModel {
    SIM_MODEL(CosmicIndifference, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        CardList selection = CardSelectCmd::FromCombatPile(ctx, CardPile::Get(PileType::Discard, Owner()), Owner(), CardSelectorPrefs(1));
        if (!selection.empty() && (selection[0].Pile().Type() == PileType::Draw || selection[0].Pile().Type() == PileType::Discard))
            CardPileCmd::Add(selection[0], PileType::Draw, CardPilePosition::Top);
    }
};
}  // namespace sts2
