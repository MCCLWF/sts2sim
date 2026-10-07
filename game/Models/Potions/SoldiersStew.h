// Models/Potions/SoldiersStew.cs
#pragma once

#include "game/Models/PotionModel.h"

namespace sts2 {

struct SoldiersStew : PotionModel {
    SIM_MODEL(SoldiersStew, PotionModel)

    void OnUse(PlayerChoiceContext&, Creature target) const {
        if (!target) return;
        for (CardModel card : target.Player().PlayerCombatState().AllCards())
            if (card.HasTag(CardTag::Strike)) card.SetBaseReplayCount(card.BaseReplayCount() + 1);
    }
};

}  // namespace sts2
