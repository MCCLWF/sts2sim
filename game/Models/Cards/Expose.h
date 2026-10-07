// Models/Cards/Expose.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Expose : CardModel {
    SIM_MODEL(Expose, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* cardPlay) const {
        if (!cardPlay || !cardPlay->Target) throw GameError("cardPlay.Target is null");
        // CreatureCmd.TriggerAnim and VfxCmd.PlayOnCreatureCenter are presentation only.
        int32_t amount = DynamicVars()["Power"].IntValue();
        Creature target = cardPlay->Target;
        CreatureCmd::LoseBlock(ctx, target, Dec(target.Block()), Owner().Creature());
        if (target.HasPower(PowerId::ARTIFACT_POWER)) PowerCmd::Remove(PowerId::ARTIFACT_POWER, target);
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, target, Dec(amount), Owner().Creature(), *this);
    }
};

}  // namespace sts2
