#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct PotionOfCapacity : PotionModel { SIM_MODEL(PotionOfCapacity, PotionModel)
    void OnUse(PlayerChoiceContext&, Creature target) const {
        if (!target || !target.Player()) throw GameError("Potion of Capacity requires a player target");
        OrbCmd::AddSlots(target.Player(), DynamicVars().Repeat().IntValue());
    }
};
}
