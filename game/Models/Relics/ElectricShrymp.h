// Models/Relics/ElectricShrymp.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/Enchantments/Imbued.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct ElectricShrymp : RelicModel {
    SIM_MODEL(ElectricShrymp, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CardList selected = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), Imbued::CanEnchant);
        for (CardModel card : selected) CardCmd::Enchant(EnchantmentId::IMBUED, card, Dec(1));
    }
};

}  // namespace sts2
