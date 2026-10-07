// Models/Relics/MassiveScroll.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Runs/CardCreationOptions.h"
namespace sts2 {
struct MassiveScroll : RelicModel {
    SIM_MODEL(MassiveScroll, RelicModel)
    bool IsAllowed(RunState runState) const { return runState.Players().size() > 1; }
    void AfterObtained() const {
        CardCreationOptionsState o = CardCreationOptions::Make({sts2::data(Owner().Character()).card_pool, CardPoolId::COLORLESS_CARD_POOL}, CardCreationSource::Other, CardRarityOddsType::RegularEncounter);
        o.filter = CardCreationOptions::OnlyMultiplayerCards;
        CardList cards = CardFactory::CreateForReward(Owner(), 3, o);
        PlayerChoiceContext context;
        CardModel chosen = CardSelectCmd::FromChooseACardScreen(context, cards, Owner(), true);
        if (chosen) CardPileCmd::Add(chosen, PileType::Deck);
    }
};
}
