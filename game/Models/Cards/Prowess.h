// Models/Cards/Prowess.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Prowess : CardModel {
    SIM_MODEL(Prowess, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(), DynamicVars().Strength().BaseValue(), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, Owner().Creature(), DynamicVars().Dexterity().BaseValue(), Owner().Creature(), *this);
    }
};
}
