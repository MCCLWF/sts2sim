// Models/Cards/UltimateStrike.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct UltimateStrike : CardModel {
    SIM_MODEL(UltimateStrike, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};
}
