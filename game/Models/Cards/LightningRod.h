// Models/Cards/LightningRod.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct LightningRod : CardModel { SIM_MODEL(LightningRod, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        PowerCmd::Apply(PowerId::LIGHTNING_ROD_POWER, ctx, Owner().Creature(),
                        DynamicVars()["LightningRodPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
