#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct CorrosiveWavePower : PowerModel {
    SIM_MODEL(CorrosiveWavePower, PowerModel)

    void AfterCardDrawn(PlayerChoiceContext& ctx, CardModel card, bool) const {
        if (!card || card.Owner().Creature() != Owner()) return;
        PowerCmd::Apply(PowerId::POISON_POWER, ctx, CombatState().HittableEnemies(), Dec(Amount()), Owner(), CardModel{});
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
