// Models/Potions/EnergyPotion.cs
#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct EnergyPotion : PotionModel {
    SIM_MODEL(EnergyPotion, PotionModel)
    void OnUse(PlayerChoiceContext&, Creature target) const {
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), target.Player());
    }
};
}
