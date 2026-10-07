// Models/Relics/LeadPaperweight.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Runs/CardCreationOptions.h"
namespace sts2 {
struct LeadPaperweight : RelicModel {
    SIM_MODEL(LeadPaperweight, RelicModel)
    void AfterObtained() const {
        CardCreationOptionsState o = CardCreationOptions::Make({CardPoolId::COLORLESS_CARD_POOL}, CardCreationSource::Other, CardRarityOddsType::RegularEncounter);
        CardList cards = CardFactory::CreateForReward(Owner(), 2, o);
        PlayerChoiceContext context;
        CardModel chosen = CardSelectCmd::FromChooseACardScreen(context, cards, Owner(), true);
        if (chosen) CardPileCmd::Add(chosen, PileType::Deck);
    }
};
}
