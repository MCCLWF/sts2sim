// Models/Potions/Fortifier.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct Fortifier : PotionModel {
    SIM_MODEL(Fortifier, PotionModel)
    void OnUse(PlayerChoiceContext&, Creature target) const {
        CreatureCmd::GainBlock(target, Dec(target.Block() * 2), ValueProp::Unpowered, nullptr);
    }
};
}
