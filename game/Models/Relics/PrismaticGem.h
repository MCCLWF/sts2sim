// Models/Relics/PrismaticGem.cs (v0.111.0).
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct PrismaticGem : RelicModel {
    SIM_MODEL(PrismaticGem, RelicModel)

    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() ? amount + Dec(DynamicVars().Energy().IntValue()) : amount;
    }

    CardCreationOptionsState ModifyCardRewardCreationOptions(
        Player player, const CardCreationOptionsState& options) const {
        if (player != Owner() || has_flag(options.flags, CardCreationFlags::NoCardPoolModifications) ||
            !has_flag(options.flags, CardCreationFlags::IsCardReward)) return options;
        bool colorlessOnly = true;
        for (CardPoolId pool : options.pools)
            colorlessOnly = colorlessOnly && pool == CardPoolId::COLORLESS_CARD_POOL;
        if (colorlessOnly) return options;

        // UnlockState.all in the oracle profile enumerates these five pools
        // in ModelDb.AllCharacters order, then appends any pools already on
        // the reward. Union keeps the first occurrence and its order.
        CardCreationOptionsState modified = options;
        constexpr CardPoolId pools[] = {CardPoolId::IRONCLAD_CARD_POOL, CardPoolId::SILENT_CARD_POOL,
                                        CardPoolId::REGENT_CARD_POOL, CardPoolId::NECROBINDER_CARD_POOL,
                                        CardPoolId::DEFECT_CARD_POOL};
        StaticVec<CardPoolId, 8> combined;
        for (CardPoolId pool : pools) if (!combined.contains(pool)) combined.push_back(pool);
        for (CardPoolId pool : options.pools) if (!combined.contains(pool)) combined.push_back(pool);
        modified.pools = combined;
        return modified;
    }
};

}  // namespace sts2
