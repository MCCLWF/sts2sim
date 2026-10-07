// Models/Cards/Feed.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Feed : CardModel {
    SIM_MODEL(Feed, CardModel)
    bool CanBeGeneratedInCombat() const { return false; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        bool shouldTriggerFatal = true;
        for (auto power : play->Target.Powers()) {
            if (!power.ShouldOwnerDeathTriggerFatal()) { shouldTriggerFatal = false; break; }
        }
        AttackCommand attack = DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        if (shouldTriggerFatal) {
            for (const auto& hit : attack.Results()) {
                for (const auto& result : hit) {
                    if (result.WasTargetKilled) {
                        CreatureCmd::GainMaxHp(Owner().Creature(), Dec(DynamicVars().MaxHp().IntValue()));
                        return;
                    }
                }
            }
        }
    }
};

}  // namespace sts2
