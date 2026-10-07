// Models/Relics/JewelryBox.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct JewelryBox : RelicModel {
    SIM_MODEL(JewelryBox, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::APOTHEOSIS, Owner()), PileType::Deck);
    }
};

}  // namespace sts2
