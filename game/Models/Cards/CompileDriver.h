// Models/Cards/CompileDriver.cs
#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct CompileDriver : CardModel { SIM_MODEL(CompileDriver, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        Dec count = DynamicVars()["CalculatedCards"].Calculate(play->Target);
        CardPileCmd::Draw(ctx, count, Owner());
    }
};
}
