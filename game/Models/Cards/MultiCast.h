#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct MultiCast : CardModel { SIM_MODEL(MultiCast, CardModel)
    bool HasEnergyCostX() const { return true; }
    sts2::OrbEvokeType OrbEvokeType() const { return sts2::OrbEvokeType::All; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        int32_t count = ResolveEnergyXValue() + (CurrentUpgradeLevel() > 0 ? 1 : 0);
        for (int32_t i = 0; i < count; ++i) OrbCmd::EvokeNext(ctx, Owner(), i == count - 1);
    }
};
}
