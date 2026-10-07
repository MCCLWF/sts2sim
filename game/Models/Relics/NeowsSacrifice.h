// Models/Relics/NeowsSacrifice.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/CardPileCmd.h"
namespace sts2 {
struct NeowsSacrifice : RelicModel {
    SIM_MODEL(NeowsSacrifice, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { PotionCmd::TryToProcure(PotionId::AMBERGRIS, Owner()); CardPileCmd::Add(Owner().RunState().CreateCard(CardId::GUILTY, Owner()), PileType::Deck); }
};
}
