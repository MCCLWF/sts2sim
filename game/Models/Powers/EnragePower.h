// Models/Powers/EnragePower.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct EnragePower : PowerModel {
    SIM_MODEL(EnragePower, PowerModel)
    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (play && play->Card.Type() == CardType::Skill)
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner(), Dec(Amount()), Owner(), CardModel{});
    }
};
}  // namespace sts2
