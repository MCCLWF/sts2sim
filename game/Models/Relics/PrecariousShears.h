// Models/Relics/PrecariousShears.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
namespace sts2 {
struct PrecariousShears : RelicModel {
    SIM_MODEL(PrecariousShears, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { CardPileCmd::RemoveFromDeck(CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(2))); PlayerChoiceContext context = PlayerChoiceContext::Throwing(); CreatureCmd::Damage(context, Owner().Creature(), Dec(16), {}, {}, {}); }
};
}
