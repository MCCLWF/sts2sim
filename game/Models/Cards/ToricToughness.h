#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
#include "game/Models/Powers/ToricToughnessPower.h"

namespace sts2 {

struct ToricToughness : CardModel {
    SIM_MODEL(ToricToughness, CardModel)

    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        Dec block = CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), cardPlay);
        PowerModel power = PowerCmd::Apply(PowerId::TORIC_TOUGHNESS_POWER, choiceContext, Owner().Creature(),
                                           DynamicVars()["Turns"].BaseValue(), Owner().Creature(), *this);
        if (power) ToricToughnessPower(power).SetBlock(block);
    }

};

}  // namespace sts2
