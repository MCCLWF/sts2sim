#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
#include "game/Models/Powers/ImitationLearningPower.h"
namespace sts2 {
struct ImitationLearning : CardModel { SIM_MODEL(ImitationLearning, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target || !play->Target.Player()) throw GameError("Imitation Learning requires a player target");
        Player target = play->Target.Player();
        for (PowerModel power : Owner().Creature().GetPowerInstances(PowerId::IMITATION_LEARNING_POWER)) {
            ImitationLearningPower imitation(power);
            if (imitation.PlayerTarget() == target) {
                PowerCmd::ModifyAmount(ctx, power, DynamicVars()["ImitationLearningPower"].BaseValue(), Owner().Creature(), *this);
                return;
            }
        }
        PowerModel power = PowerCmd::Apply(PowerId::IMITATION_LEARNING_POWER, ctx, Owner().Creature(),
            DynamicVars()["ImitationLearningPower"].BaseValue(), Owner().Creature(), *this);
        if (power) ImitationLearningPower(power).SetPlayerTarget(target);
    }
};
}
