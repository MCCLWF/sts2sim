// Models/Potions/StarPotion.cs
#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct StarPotion : PotionModel {
    SIM_MODEL(StarPotion, PotionModel)
    void OnUse(PlayerChoiceContext&, Creature target) const {
        PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), target.Player());
    }
};
}
