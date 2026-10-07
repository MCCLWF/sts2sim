#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {
struct GalacticDust : RelicModel {
    SIM_MODEL(GalacticDust, RelicModel)
    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const {
        const int32_t threshold = DynamicVars().Stars().IntValue();
        return threshold > 0 ? extra(0) % threshold : 0;
    }
    void AfterStarsSpent(int32_t amount, Player spender) const {
        if (spender != Owner() || amount <= 0) return;
        const int32_t threshold = DynamicVars().Stars().IntValue();
        if (threshold <= 0) return;
        extra(0) += amount;
        int32_t activations = extra(0) / threshold;
        if (activations <= 0) return;
        int32_t block = DynamicVars().Block().IntValue() * activations;
        extra(0) %= threshold;
        CreatureCmd::GainBlock(Owner().Creature(), Dec(block), ValueProp::Unpowered, nullptr);
    }
};
}  // namespace sts2
