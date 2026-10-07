#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Voltaic : CardModel { SIM_MODEL(Voltaic, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        int32_t count = DynamicVars().CalculationBase().IntValue() +
            DynamicVars().CalculationExtra().IntValue() * CombatState().st().player_lightning_orbs_channeled_this_combat;
        for (int32_t i = 0; i < count; ++i) OrbCmd::Channel(OrbId::LIGHTNING_ORB, ctx, Owner());
    }
};
}
