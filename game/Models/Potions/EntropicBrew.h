#pragma once

#include "game/Commands/PotionCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct EntropicBrew : PotionModel {
    SIM_MODEL(EntropicBrew, PotionModel)

    void OnUse(PlayerChoiceContext&, Creature target) const {
        if (!target) return;
        Player player = target.Player();
        while (player.HasOpenPotionSlots()) {
            if (!PotionCmd::TryToProcure(PotionFactory::CreateRandomPotionOutOfCombat(
                                            player, player.RunState().Rng().CombatPotionGeneration()), player).success) {
                break;
            }
        }
    }
};

}  // namespace sts2
