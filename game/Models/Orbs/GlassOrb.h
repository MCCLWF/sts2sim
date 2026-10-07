// Models/Orbs/GlassOrb.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/OrbModel.h"
namespace sts2 {
struct GlassOrb : OrbModel { SIM_MODEL(GlassOrb, OrbModel)
    Dec PassiveVal() const { return ModifyOrbValue(st().glass_passive_val); }
    Dec EvokeVal() const { return PassiveVal() * Dec(2); }
    void BeforeTurnEndOrbTrigger(PlayerChoiceContext& ctx) const { TriggerPassive(ctx, Creature{}); }
    void Passive(PlayerChoiceContext& ctx, Creature) const {
        CreatureList targets;
        for (Creature enemy : CombatState().HittableEnemies()) if (enemy.IsHittable()) targets.push_back(enemy);
        Dec passive = PassiveVal();
        if (passive <= Dec(0)) return;
        st().glass_passive_val = st().glass_passive_val > Dec(0) ? st().glass_passive_val - Dec(1) : Dec(0);
        CreatureCmd::Damage(ctx, targets, passive, ValueProp::Unpowered, Owner().Creature());
    }
    CreatureList Evoke(PlayerChoiceContext& ctx) const {
        CreatureList targets;
        for (Creature enemy : CombatState().HittableEnemies()) if (enemy.IsHittable()) targets.push_back(enemy);
        Dec amount = EvokeVal();
        if (amount <= Dec(0)) return {};
        CreatureCmd::Damage(ctx, targets, amount, ValueProp::Unpowered, Owner().Creature());
        return targets;
    }
};
}
