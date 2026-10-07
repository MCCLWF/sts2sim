#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct SpinnerPower : PowerModel { SIM_MODEL(SpinnerPower, PowerModel)
    void AfterEnergyReset(Player player) const {
        if (player != Owner().Player()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        for (int32_t i = 0; i < Amount(); ++i) OrbCmd::Channel(OrbId::GLASS_ORB, ctx, player);
    }
};
}
