#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct OrbitPower : PowerModel {
    SIM_MODEL(OrbitPower, PowerModel)
    void AfterEnergySpent(CardModel card, int32_t amount) const {
        if (card.Owner().Creature() != Owner() || amount <= 0) return;
        const int32_t threshold = DynamicVars().Energy().IntValue();
        if (threshold <= 0) return;
        extra(0) += amount;
        int32_t triggers = extra(0) / threshold - extra(1);
        if (triggers > 0) {
            PlayerCmd::GainEnergy(Dec(Amount() * triggers), Owner().Player());
            extra(1) += triggers;
        }
    }
};
}  // namespace sts2
