// Models/Relics/PhialHolster.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/PotionCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct PhialHolster : RelicModel {
    SIM_MODEL(PhialHolster, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const {
        PlayerCmd::GainMaxPotionCount(1, Owner());
        for (PotionId id : PotionFactory::CreateRandomPotionsOutOfCombat(Owner(), 2, Owner().RunState().Rng().CombatPotionGeneration())) PotionCmd::TryToProcure(id, Owner());
    }
};
}
