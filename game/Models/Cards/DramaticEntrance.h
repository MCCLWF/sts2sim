// Models/Cards/DramaticEntrance.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct DramaticEntrance : CardModel {
    SIM_MODEL(DramaticEntrance, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
    }
};
}
