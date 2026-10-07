// Models/Orbs/FrostOrb.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/OrbModel.h"
namespace sts2 {
struct FrostOrb : OrbModel { SIM_MODEL(FrostOrb, OrbModel)
    Dec PassiveVal() const { return ModifyOrbValue(Dec(2)); }
    Dec EvokeVal() const { return ModifyOrbValue(Dec(5)); }
    void BeforeTurnEndOrbTrigger(PlayerChoiceContext& ctx) const { TriggerPassive(ctx, Creature{}); }
    void Passive(PlayerChoiceContext&, Creature target) const {
        if (target) throw GameError("Frost orbs cannot target creatures.");
        CreatureCmd::GainBlock(Owner().Creature(), PassiveVal(), ValueProp::Unpowered, nullptr);
        // Hibernate's multiplayer sharing has no other player in this simulator.
    }
    CreatureList Evoke(PlayerChoiceContext&) const {
        Creature own = Owner().Creature();
        CreatureCmd::GainBlock(own, EvokeVal(), ValueProp::Unpowered, nullptr);
        return CreatureList{own};
    }
};
}
