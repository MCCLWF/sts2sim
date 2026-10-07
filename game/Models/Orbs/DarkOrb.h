// Models/Orbs/DarkOrb.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/OrbModel.h"
namespace sts2 {
struct DarkOrb : OrbModel { SIM_MODEL(DarkOrb, OrbModel)
    Dec PassiveVal() const { return ModifyOrbValue(Dec(6)); }
    Dec EvokeVal() const { return st().dark_evoke_val; }
    void BeforeTurnEndOrbTrigger(PlayerChoiceContext& ctx) const { TriggerPassive(ctx, Creature{}); }
    void Passive(PlayerChoiceContext&, Creature target) const {
        if (target) throw GameError("Dark orbs cannot target creatures.");
        st().dark_evoke_val = st().dark_evoke_val + PassiveVal();
    }
    CreatureList Evoke(PlayerChoiceContext& ctx) const {
        CreatureList enemies = CombatState().HittableEnemies();
        if (enemies.empty()) return {};
        Creature weakest = enemies.front();
        for (Creature enemy : enemies) {
            if (enemy.CurrentHp() < weakest.CurrentHp()) weakest = enemy;
        }
        CreatureCmd::Damage(ctx, weakest, EvokeVal(), ValueProp::Unpowered, Owner().Creature());
        return CreatureList{weakest};
    }
};
}
