// Models/Potions/BlockPotion.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct BlockPotion : PotionModel {
    SIM_MODEL(BlockPotion, PotionModel)
    void OnUse(PlayerChoiceContext&, Creature target) const {
        CreatureCmd::GainBlock(target, DynamicVars().Block().BaseValue(), DynamicVars().Block().Props(), nullptr);
    }
};
}
