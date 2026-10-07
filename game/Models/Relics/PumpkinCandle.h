// Models/Relics/PumpkinCandle.cs (v0.111.0).
#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct PumpkinCandle : RelicModel {
    SIM_MODEL(PumpkinCandle, RelicModel)

    static constexpr int32_t kKindleAmount = 5;

    int32_t KindleCount() const { return extra(0); }

    void SetKindleCount(int32_t count) const {
        extra(0) = count;
        SetStatus(count <= 0 ? RelicStatus::Disabled : RelicStatus::Normal);
    }

    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return KindleCount(); }

    void AfterObtained() const { Rekindle(); }

    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        if (player != Owner() || KindleCount() <= 0) return amount;
        return amount + Dec(DynamicVars().Energy().IntValue());
    }

    void AfterCombatEnd(CombatRoom) const {
        SetKindleCount(KindleCount() > 0 ? KindleCount() - 1 : 0);
    }

    void Rekindle() const { SetKindleCount(KindleCount() + kKindleAmount); }
};

}  // namespace sts2
