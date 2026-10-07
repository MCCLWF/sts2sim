#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Tempest : CardModel { SIM_MODEL(Tempest, CardModel)
    bool HasEnergyCostX() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        int32_t count = ResolveEnergyXValue() + (CurrentUpgradeLevel() > 0 ? 1 : 0);
        for (int32_t i = 0; i < count; ++i) OrbCmd::Channel(OrbId::LIGHTNING_ORB, ctx, Owner());
    }
};
}
