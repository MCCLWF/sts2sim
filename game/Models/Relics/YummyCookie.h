// Models/Relics/YummyCookie.cs.
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct YummyCookie : RelicModel {
    SIM_MODEL(YummyCookie, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardList cards = CardSelectCmd::FromDeckForUpgrade(Owner(), CardSelectorPrefs(DynamicVars().Cards().IntValue()));
        CardCmd::Upgrade(cards);
    }
};

}  // namespace sts2
