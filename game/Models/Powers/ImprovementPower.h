#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Models/PowerModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct ImprovementPower : PowerModel {
    SIM_MODEL(ImprovementPower, PowerModel)

    void AfterCombatEnd(CombatRoom) const {
        Player player = Owner().Player();
        if (!player) return;
        CardList upgradable;
        for (CardModel card : CardPile::Get(PileType::Deck, player).Cards())
            if (card.IsUpgradable()) upgradable.push_back(card);
        for (int32_t i = 0; i < Amount() && !upgradable.empty(); ++i) {
            const int32_t index = player.RunState().Rng().CombatCardSelection().next_item_index(
                static_cast<int32_t>(upgradable.size()));
            if (index < 0) break;
            CardModel card = upgradable[static_cast<size_t>(index)];
            upgradable.erase_at(static_cast<size_t>(index));
            CardCmd::Upgrade(card);
        }
    }
};

}  // namespace sts2
