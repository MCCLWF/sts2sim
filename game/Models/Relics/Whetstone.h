// Models/Relics/Whetstone.cs
#pragma once

#include <algorithm>
#include "core/dotnet_sort.h"
#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Whetstone : RelicModel {
    SIM_MODEL(Whetstone, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const {
        CardList attacks;
        for (CardModel card : Owner().Deck().Cards())
            if (card.Type() == CardType::Attack && card.IsUpgradable()) attacks.push_back(card);
        dotnet_sort(attacks.span(), [](CardModel a, CardModel b) { return a.CompareTo(b); });
        Owner().RunState().Rng().Niche().Shuffle(attacks);
        int32_t count = std::min<int32_t>(DynamicVars().Cards().IntValue(), static_cast<int32_t>(attacks.size()));
        for (int32_t i = 0; i < count; ++i) CardCmd::Upgrade(attacks[static_cast<size_t>(i)]);
    }
};

}  // namespace sts2
