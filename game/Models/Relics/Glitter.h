// Models/Relics/Glitter.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Glitter : RelicModel {
    SIM_MODEL(Glitter, RelicModel)

    bool TryModifyCardRewardOptionsLate(Player player, CardList& cards,
                                        const CardCreationOptionsState&) const {
        if (player != Owner()) return false;
        for (CardModel& card : cards) {
            if (card.Type() == CardType::Status || card.Type() == CardType::Curse || card.Type() == CardType::Quest ||
                (card.Pile() && card.Pile().Type() == PileType::Deck && card.Keywords().Contains(CardKeyword::Unplayable)) ||
                card.Enchantment()) continue;
            CardModel glam = Owner().RunState().CloneCard(card);
            CardCmd::Enchant(EnchantmentId::GLAM, glam, Dec(1));
            card = glam;
        }
        return true;
    }
};

}  // namespace sts2
