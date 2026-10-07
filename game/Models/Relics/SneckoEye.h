// Models/Relics/SneckoEye.cs.
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/Powers/ConfusedPower.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct SneckoEye : RelicModel {
    SIM_MODEL(SneckoEye, RelicModel)

    void AfterObtained() const {
        if (CombatManager(sim_).IsInProgress()) ApplyConfusion();
    }

    void BeforeCombatStart() const { ApplyConfusion(); }

    Dec ModifyHandDraw(Player player, Dec count) const {
        return player == Owner() ? count + DynamicVars().Cards().BaseValue() : count;
    }

private:
    void ApplyConfusion() const {
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::CONFUSED_POWER, context, Owner().Creature(), Dec(1), Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
