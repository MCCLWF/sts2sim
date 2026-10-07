// Models/Relics/AlchemicalCoffer.cs.
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct AlchemicalCoffer : RelicModel {
    SIM_MODEL(AlchemicalCoffer, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        Player owner = Owner();
        int32_t firstNewSlot = owner.MaxPotionCount();
        int32_t count = DynamicVars()["PotionSlots"].IntValue();
        PlayerCmd::GainMaxPotionCount(count, owner);
        StaticVec<PotionId, 8> potions = PotionFactory::CreateRandomPotionsOutOfCombat(
            owner, count, owner.RunState().Rng().CombatPotionGeneration());
        for (int32_t i = 0; i < count; ++i) {
            PotionCmd::TryToProcure(potions[static_cast<size_t>(i)], owner, firstNewSlot + i);
        }
    }
};

}  // namespace sts2
