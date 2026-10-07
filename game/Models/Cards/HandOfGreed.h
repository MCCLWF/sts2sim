#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct HandOfGreed : CardModel {
    SIM_MODEL(HandOfGreed, CardModel)
    bool CanBeGeneratedInCombat() const { return false; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("HandOfGreed target is null");
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
                        PlayerCmd::GainGold(DynamicVars()["Gold"].IntValue(), Owner());
                        return;
                    }
                }
            }
        }
    }
};

}  // namespace sts2
