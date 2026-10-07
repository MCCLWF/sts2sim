#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct BulkUp : CardModel { SIM_MODEL(BulkUp, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        OrbCmd::RemoveSlots(Owner(), DynamicVars()["OrbSlots"].IntValue());
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(), DynamicVars().Strength().BaseValue(), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Owner().Creature(), DynamicVars().Dexterity().BaseValue(), Owner().Creature(), *this);
    }
};
}
