// Models/Relics/DingyRug.cs
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct DingyRug : RelicModel {
    SIM_MODEL(DingyRug, RelicModel)

    CardCreationOptionsState ModifyCardRewardCreationOptions(Player player,
                                                              const CardCreationOptionsState& options) const {
        if (player != Owner() || has_flag(options.flags, CardCreationFlags::NoCardPoolModifications) ||
            !has_flag(options.flags, CardCreationFlags::IsCardReward) ||
            options.pools.contains(CardPoolId::COLORLESS_CARD_POOL)) return options;
        CardCreationOptionsState modified = options;
        modified.pools.push_back(CardPoolId::COLORLESS_CARD_POOL);
        return modified;
    }
};

}  // namespace sts2
