#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct TrashToTreasure : CardModel { SIM_MODEL(TrashToTreasure, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::TRASH_TO_TREASURE_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};
}
