// Models/Relics/PaelsHorn.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct PaelsHorn : RelicModel {
    SIM_MODEL(PaelsHorn, RelicModel)

    void AfterObtained() const {
        for (int i = 0; i < 2; ++i) {
            CardModel card = Owner().RunState().CreateCard(CardId::RELAX, Owner());
            CardPileCmd::Add(card, PileType::Deck);
        }
    }
};

}  // namespace sts2
