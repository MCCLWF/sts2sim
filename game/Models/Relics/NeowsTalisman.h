// Models/Relics/NeowsTalisman.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CardCmd.h"
namespace sts2 {
struct NeowsTalisman : RelicModel {
    SIM_MODEL(NeowsTalisman, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const {
        CardModel strike, defend;
        for (CardModel c : Owner().Deck().Cards()) if (c.Rarity() == CardRarity::Basic) { if (c.HasTag(CardTag::Strike)) strike = c; if (c.HasTag(CardTag::Defend)) defend = c; }
        if (strike) CardCmd::Upgrade(strike);
        if (defend) CardCmd::Upgrade(defend);
    }
};
}
