// Models/Cards/Guilty.cs
#pragma once
#include "game/Models/CardModel.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Rooms/AbstractRoom.h"
namespace sts2 {
struct Guilty : CardModel {
    SIM_MODEL(Guilty, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    void AfterCombatEnd(CombatRoom room) const { (void)room; if (Pile().Type() != PileType::Deck) return; ++extra(0); if (extra(0) >= 5) CardPileCmd::RemoveFromDeck(*this); }
};
}
