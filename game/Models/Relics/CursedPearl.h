// Models/Relics/CursedPearl.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct CursedPearl : RelicModel {
    SIM_MODEL(CursedPearl, RelicModel)
    void AfterObtained() const { CardPileCmd::AddCurseToDeck(CardId::GREED, Owner()); PlayerCmd::GainGold(Dec(333), Owner()); }
};
}
