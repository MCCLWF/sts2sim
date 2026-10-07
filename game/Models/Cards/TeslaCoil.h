#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct TeslaCoil : CardModel { SIM_MODEL(TeslaCoil, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        auto orbs = Owner().PlayerCombatState().st().orbs;
        for (OrbRef ref : orbs) {
            OrbModel orb(sim_, ref);
            if (orb.OrbId_() != OrbId::LIGHTNING_ORB) continue;
            OrbCmd::Passive(ctx, orb, play->Target);
            if (CurrentUpgradeLevel() > 0) OrbCmd::Passive(ctx, orb, play->Target);
        }
    }
};
}
