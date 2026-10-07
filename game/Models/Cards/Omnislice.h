// Models/Cards/Omnislice.cs
#pragma once

#include "game/Commands/Builders/AttackContext.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Omnislice : CardModel {
    SIM_MODEL(Omnislice, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        AttackContext attack(CombatState(), ctx, *this, play);
        attack.Run([&] {
            DamageResults results = CreatureCmd::Damage(ctx, play->Target, DynamicVars().Damage().BaseValue(),
                                                        ValueProp::Move, *this, play);
            attack.AddHit(results);
            if (results.empty()) return;
            CreatureList others;
            for (Creature teammate : CombatState().GetTeammatesOf(results[0].Receiver))
                if (teammate != play->Target && teammate.IsHittable()) others.push_back(teammate);
            if (others.empty()) return;
            int32_t splash = results[0].TotalDamage() + results[0].OverkillDamage;
            DamageResults splashResults = CreatureCmd::Damage(ctx, others, Dec(splash),
                ValueProp::Unpowered | ValueProp::Move, Owner().Creature(), *this, play);
            attack.AddHit(splashResults);
        });
    }
};

}  // namespace sts2
