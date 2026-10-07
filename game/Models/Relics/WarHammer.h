// Models/Relics/WarHammer.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct WarHammer : RelicModel {
    SIM_MODEL(WarHammer, RelicModel)

    void AfterCombatVictory(CombatRoom room) const {
        if (room.RoomType() != RoomType::Elite) return;
        CardList cards;
        for (CardModel card : Owner().Deck().Cards()) if (card.IsUpgradable()) cards.push_back(card);
        CardPileCmd::StableShuffle(cards, Owner().RunState().Rng().Niche());
        int32_t count = std::min(DynamicVars().Cards().IntValue(), static_cast<int32_t>(cards.size()));
        for (int32_t i = 0; i < count; ++i) CardCmd::Upgrade(cards[static_cast<size_t>(i)]);
    }
};

}  // namespace sts2
