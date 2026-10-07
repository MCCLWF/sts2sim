// Models/Powers/LightningRodPower.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct LightningRodPower : PowerModel { SIM_MODEL(LightningRodPower, PowerModel)
    void AfterEnergyReset(Player player) const {
        if (player != Owner().Player()) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        OrbCmd::Channel(OrbId::LIGHTNING_ORB, ctx, player);
        PowerCmd::Decrement(*this);
    }
};
}
