// Models/Orbs/LightningOrb.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/OrbModel.h"
namespace sts2 {
struct LightningOrb : OrbModel { SIM_MODEL(LightningOrb, OrbModel)
    Dec PassiveVal() const { return ModifyOrbValue(Dec(3)); }
    Dec EvokeVal() const { return ModifyOrbValue(Dec(8)); }
    void BeforeTurnEndOrbTrigger(PlayerChoiceContext& ctx) const { TriggerPassive(ctx, Creature{}); }
    void Passive(PlayerChoiceContext& ctx, Creature target) const {
        CreatureList enemies;
        for (Creature enemy : CombatState().GetOpponentsOf(Owner().Creature())) {
            if (enemy.IsHittable()) enemies.push_back(enemy);
        }
        if (enemies.empty()) return;
        Creature chosen = target ? target : Owner().RunState().Rng().CombatTargets().NextItem(enemies);
        CreatureCmd::Damage(ctx, CreatureList{chosen}, PassiveVal(), ValueProp::Unpowered, Owner().Creature());
    }
    CreatureList Evoke(PlayerChoiceContext& ctx) const {
        CreatureList enemies;
        for (Creature enemy : CombatState().GetOpponentsOf(Owner().Creature())) {
            if (enemy.IsHittable()) enemies.push_back(enemy);
        }
        if (enemies.empty()) return {};
        Creature chosen = Owner().RunState().Rng().CombatTargets().NextItem(enemies);
        CreatureList targets{chosen};
        CreatureCmd::Damage(ctx, targets, EvokeVal(), ValueProp::Unpowered, Owner().Creature());
        return targets;
    }
};
}
