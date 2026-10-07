#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Subroutine : CardModel { SIM_MODEL(Subroutine, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::SUBROUTINE_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};
}
