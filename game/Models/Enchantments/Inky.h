#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/EnchantmentModel.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Inky : EnchantmentModel {
    SIM_MODEL(Inky, EnchantmentModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CardModel card = Card();
        CreatureList targets;
        if (card.TargetType() != sts2::TargetType::AllEnemies) {
            if (!play) throw GameError("Inky.OnPlay requires CardPlay");
            targets.push_back(play->Target);
        } else {
            targets = card.CombatState().HittableEnemies();
        }
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets,
                        DynamicVarSet(*this)["WeakPower"].BaseValue(), card.Owner().Creature(), card);
    }
};

}  // namespace sts2
