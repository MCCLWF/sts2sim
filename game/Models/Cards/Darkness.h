// Models/Cards/Darkness.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Darkness : CardModel { SIM_MODEL(Darkness, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        OrbCmd::Channel(OrbId::DARK_ORB, ctx, Owner());
        for (OrbRef ref : Owner().PlayerCombatState().st().orbs) {
            OrbModel orb(&sim(), ref);
            if (orb.OrbId_() == OrbId::DARK_ORB)
                for (int i = 0; i < (CurrentUpgradeLevel() > 0 ? 2 : 1); ++i) OrbCmd::Passive(ctx, orb, Creature{});
        }
    }
};
}
