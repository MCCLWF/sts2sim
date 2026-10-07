#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct BootSequence : CardModel { SIM_MODEL(BootSequence, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* play) const { CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play); }
};
}
