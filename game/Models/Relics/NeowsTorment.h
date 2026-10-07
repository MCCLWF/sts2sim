// Models/Relics/NeowsTorment.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardPileCmd.h"
namespace sts2 {
struct NeowsTorment : RelicModel {
    SIM_MODEL(NeowsTorment, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { CardModel card = Owner().RunState().CreateCard(CardId::NEOWS_FURY, Owner()); CardPileCmd::Add(card, PileType::Deck); }
};
}
