// Models/Relics/PaelsClaw.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/Enchantments/Goopy.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PaelsClaw : RelicModel {
    SIM_MODEL(PaelsClaw, RelicModel)

    void AfterObtained() const {
        const CardList deck = Owner().Deck().Cards();
        for (CardModel card : deck)
            if (Goopy::CanEnchant(card)) CardCmd::Enchant(EnchantmentId::GOOPY, card, Dec(1));
    }
};

}  // namespace sts2
