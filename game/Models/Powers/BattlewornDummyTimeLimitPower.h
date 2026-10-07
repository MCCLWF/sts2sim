// Models/Powers/BattlewornDummyTimeLimitPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct BattlewornDummyTimeLimitPower : PowerModel {
    SIM_MODEL(BattlewornDummyTimeLimitPower, PowerModel)

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        if (Amount() > 1) {
            PowerCmd::Decrement(*this);
            return;
        }
        CombatData& combat = gs().combat;
        switch (combat.encounter) {
            case EncounterId::BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER:
            case EncounterId::BATTLEWORN_DUMMY_EVENT_V2_ENCOUNTER:
            case EncounterId::BATTLEWORN_DUMMY_EVENT_V3_ENCOUNTER:
                combat.battleworn_dummy_ran_out_of_time = true;
                break;
            default:
                break;
        }
        CreatureCmd::Escape(Owner());
    }
};

}  // namespace sts2
