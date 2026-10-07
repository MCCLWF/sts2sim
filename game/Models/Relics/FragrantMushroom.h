// Models/Relics/FragrantMushroom.cs
#pragma once

#include <algorithm>

#include "game/Commands/CardCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct FragrantMushroom : RelicModel {
    SIM_MODEL(FragrantMushroom, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().HpLoss().BaseValue(),
                            ValueProp::Unblockable | ValueProp::Unpowered, Creature{});
        CardList upgradable;
        for (CardModel card : Owner().Deck().Cards())
            if (card.IsUpgradable()) upgradable.push_back(card);
        CardPileCmd::StableShuffle(upgradable, Owner().RunState().Rng().Niche());
        int32_t count = std::min<int32_t>(DynamicVars().Cards().IntValue(), static_cast<int32_t>(upgradable.size()));
        for (int32_t i = 0; i < count; ++i) CardCmd::Upgrade(upgradable[static_cast<size_t>(i)]);
    }
};

}  // namespace sts2
