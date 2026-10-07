#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct CoolantPower : PowerModel { SIM_MODEL(CoolantPower, PowerModel)
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (!participants.contains(Owner())) return;
        auto& orbs = Owner().Player().PlayerCombatState().st().orbs;
        OrbId unique[5]{};
        int32_t count = 0;
        for (OrbRef ref : orbs) {
            OrbId id = OrbModel(sim_, ref).OrbId_();
            bool seen = false;
            for (int32_t i = 0; i < count; ++i) seen |= unique[i] == id;
            if (!seen && count < 5) unique[count++] = id;
        }
        if (count) CreatureCmd::GainBlock(Owner(), Dec(count * Amount()), ValueProp::Unpowered, nullptr);
    }
};
}
