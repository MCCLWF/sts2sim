// Models/Relics/WarPaint.cs
#pragma once

#include <algorithm>
#include "core/dotnet_sort.h"

#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct WarPaint : RelicModel {
    SIM_MODEL(WarPaint, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        // StableShuffle: sort by CardModel.CompareTo, then shuffle with Niche.
        CardList skills;
        for (CardModel card : Owner().Deck().Cards()) {
            if (card.Type() == CardType::Skill && card.IsUpgradable()) skills.push_back(card);
        }
        dotnet_sort(skills.span(), [](CardModel a, CardModel b) { return a.CompareTo(b); });
        Owner().RunState().Rng().Niche().Shuffle(skills);
        int32_t count = std::min<int32_t>(2, static_cast<int32_t>(skills.size()));
        for (int32_t i = 0; i < count; ++i) CardCmd::Upgrade(skills[static_cast<size_t>(i)]);
    }
};

}  // namespace sts2
