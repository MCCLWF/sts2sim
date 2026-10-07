// Models/Orbs/PlasmaOrb.cs
#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/OrbModel.h"
namespace sts2 {
struct PlasmaOrb : OrbModel { SIM_MODEL(PlasmaOrb, OrbModel)
    Dec PassiveVal() const { return Dec(1); }
    Dec EvokeVal() const { return Dec(2); }
    void AfterTurnStartOrbTrigger(PlayerChoiceContext& ctx) const { TriggerPassive(ctx, Creature{}); }
    void Passive(PlayerChoiceContext&, Creature target) const {
        if (target) throw GameError("Plasma orbs cannot target creatures.");
        PlayerCmd::GainEnergy(PassiveVal(), Owner());
    }
    CreatureList Evoke(PlayerChoiceContext&) const {
        PlayerCmd::GainEnergy(EvokeVal(), Owner());
        return CreatureList{Owner().Creature()};
    }
};
}
