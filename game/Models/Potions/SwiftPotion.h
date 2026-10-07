// Models/Potions/SwiftPotion.cs
#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct SwiftPotion : PotionModel {
    SIM_MODEL(SwiftPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), target.Player());
    }
};
}
