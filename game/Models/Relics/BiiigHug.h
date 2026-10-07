// Models/Relics/BiiigHug.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BiiigHug : RelicModel {
    SIM_MODEL(BiiigHug, RelicModel)

    void AfterObtained() const {
        CardList cards = CardSelectCmd::FromDeckForRemoval(
            Owner(), CardSelectorPrefs(DynamicVars().Cards().IntValue()));
        CardPileCmd::RemoveFromDeck(cards);
    }

    void AfterShuffle(PlayerChoiceContext&, Player shuffler) const {
        if (shuffler != Owner()) return;
        CardModel soot = shuffler.Creature().CombatState().CreateCard(CardId::SOOT, Owner());
        CardPileCmd::AddGeneratedCardToCombat(soot, PileType::Draw, Owner(), CardPilePosition::Random);
        Flash();
        // Unported presentation: CardCmd.Preview(soot, 0.75f) and Cmd.Wait(1f).
    }
};

}  // namespace sts2
