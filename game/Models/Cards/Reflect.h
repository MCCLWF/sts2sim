#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Reflect : CardModel {
    SIM_MODEL(Reflect, CardModel)
    int32_t CanonicalStarCost() const { return 3; }
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        PowerCmd::Apply(PowerId::REFLECT_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};
}  // namespace sts2
