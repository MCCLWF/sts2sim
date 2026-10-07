// Models/Relics/TanxsWhistle.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct TanxsWhistle : RelicModel {
    SIM_MODEL(TanxsWhistle, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::WHISTLE, Owner()), PileType::Deck);
    }
};

}  // namespace sts2
