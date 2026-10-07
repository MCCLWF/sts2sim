// Models/Potions/ShipInABottle.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct ShipInABottle : PotionModel {
    SIM_MODEL(ShipInABottle, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        CreatureCmd::GainBlock(target, DynamicVars().Block().BaseValue(), DynamicVars().Block().Props(), nullptr);
        PowerCmd::Apply(PowerId::BLOCK_NEXT_TURN_POWER, ctx, target, DynamicVars().Block().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
