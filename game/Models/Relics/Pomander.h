// Models/Relics/Pomander.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
namespace sts2 {
struct Pomander : RelicModel {
    SIM_MODEL(Pomander, RelicModel)
    void AfterObtained() const { CardList cards = CardSelectCmd::FromDeckForUpgrade(Owner(), CardSelectorPrefs(1)); for (CardModel c : cards) CardCmd::Upgrade(c); }
};
}
