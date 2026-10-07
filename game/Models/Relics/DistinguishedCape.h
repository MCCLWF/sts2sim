// Models/Relics/DistinguishedCape.cs (v0.111.0).
#pragma once

#include <algorithm>

#include "game/Commands/CardPileCmd.h"
#include "game/Models/Canonical.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct DistinguishedCape : RelicModel {
    SIM_MODEL(DistinguishedCape, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        StaticVec<CardId, 32> curses;
        for (CardId id : sts2::data(CardPoolId::CURSE_CARD_POOL).cards) {
            CanonicalCard canonical(&sim(), id);
            if (canonical.get().CanBeGeneratedByModifiers()) curses.push_back(id);
        }
        std::sort(curses.begin(), curses.end(), [](CardId a, CardId b) {
            return sts2::data(a).entry < sts2::data(b).entry;
        });
        for (int32_t i = 0; i < DynamicVars()["Curses"].IntValue() && !curses.empty(); ++i) {
            CardId id = Owner().RunState().Rng().Niche().NextItem(curses);
            curses.remove(id);
            CardPileCmd::Add(Owner().RunState().CreateCard(id, Owner()), PileType::Deck);
        }
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i)
            CardPileCmd::Add(Owner().RunState().CreateCard(CardId::APPARITION, Owner()), PileType::Deck);
    }
};

}  // namespace sts2
