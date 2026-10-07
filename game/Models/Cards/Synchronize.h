#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Synchronize : CardModel { SIM_MODEL(Synchronize, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        StaticVec<OrbId, 10> ids;
        for (OrbRef ref : Owner().PlayerCombatState().st().orbs) {
            OrbId id = OrbModel(sim_, ref).OrbId_();
            if (!ids.contains(id)) ids.push_back(id);
        }
        // Each distinct orb type contributes CalculationExtra to CalculatedFocus.
        int32_t focus = DynamicVars().CalculationBase().IntValue() + DynamicVars().CalculationExtra().IntValue() * static_cast<int32_t>(ids.size());
        PowerCmd::Apply(PowerId::SYNCHRONIZE_POWER, ctx, Owner().Creature(), Dec(focus), Owner().Creature(), *this);
    }
};
}
