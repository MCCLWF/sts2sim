// Models/Relics/DowsingRod.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardPileCmd.h"
namespace sts2 {
struct DowsingRod : RelicModel {
    SIM_MODEL(DowsingRod, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { CardPileCmd::Add(Owner().RunState().CreateCard(CardId::DOWSING, Owner()), PileType::Deck); }
};
}
