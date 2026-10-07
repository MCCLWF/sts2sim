#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct HelicalDart : RelicModel {
    SIM_MODEL(HelicalDart, RelicModel)

    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !play->Card.HasTag(CardTag::Shiv)) return;
        PowerCmd::Apply(PowerId::HELICAL_DART_POWER, ctx, Owner().Creature(), DynamicVars().Dexterity().IntValue(),
                        Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
