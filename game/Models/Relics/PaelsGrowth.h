// Models/Relics/PaelsGrowth.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PaelsGrowth : RelicModel {
    SIM_MODEL(PaelsGrowth, RelicModel)

    void AfterObtained() const {
        CardList selected = CardSelectCmd::FromDeckForEnchantment(
            Owner(), CardSelectorPrefs(1), event_allowed::BaseCanEnchant);
        for (CardModel card : selected)
            CardCmd::Enchant(EnchantmentId::CLONE, card, Dec(4));
    }
    // TryModifyRestSiteOptions is a guarded hook represented by
    // RestSite::Generate, which appends Clone in listener order.
};

}  // namespace sts2
