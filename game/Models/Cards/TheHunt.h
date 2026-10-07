// Models/Cards/TheHunt.cs (v0.111.0).
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
#include "game/Rewards/Reward.h"
#include "game/Runs/CardCreationOptions.h"

namespace sts2 {

struct TheHunt : CardModel {
    SIM_MODEL(TheHunt, CardModel)

    bool CanBeGeneratedInCombat() const { return false; }

    void OnPlay(PlayerChoiceContext& context, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("The Hunt target is null");
        bool fatalAllowed = true;
        for (PowerModel power : play->Target.Powers()) {
            if (!power.ShouldOwnerDeathTriggerFatal()) { fatalAllowed = false; break; }
        }
        AttackCommand attack = DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .FromCard(*this, play).Targeting(play->Target).WithHitFx("vfx/vfx_attack_slash").Execute(context);
        if (!fatalAllowed) return;
        for (const auto& hit : attack.Results()) {
            for (const DamageResult& result : hit) {
                if (!result.WasTargetKilled) continue;
                CardCreationOptionsState options = CardCreationOptions::ForRoom(Owner(), sim_->g->run.rooms.back());
                Reward reward = Reward::NewCardReward(Owner(), options, 3);
                sim_->g->run.combat_extra_rewards.push_back(reward.Index());
                PowerCmd::Apply(PowerId::THE_HUNT_POWER, context, Owner().Creature(), Dec(1),
                                Owner().Creature(), *this);
                return;
            }
        }
    }
};

}  // namespace sts2
