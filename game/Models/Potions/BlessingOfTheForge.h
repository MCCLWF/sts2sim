#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct BlessingOfTheForge : PotionModel {
    SIM_MODEL(BlessingOfTheForge, PotionModel)

    void OnUse(PlayerChoiceContext&, Creature target) const {
        for (CardModel card : target.Player().PlayerCombatState().Hand().Cards()) {
            if (card.IsUpgradable()) CardCmd::Upgrade(card);
        }
    }
};

}  // namespace sts2
