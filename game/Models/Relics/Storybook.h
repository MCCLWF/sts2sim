// Models/Relics/Storybook.cs.
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Storybook : RelicModel {
    SIM_MODEL(Storybook, RelicModel)

    void AfterObtained() const {
        CardModel card = Owner().RunState().CreateCard(CardId::BRIGHTEST_FLAME, Owner());
        CardPileCmd::Add(card, PileType::Deck);
    }
};

}  // namespace sts2
