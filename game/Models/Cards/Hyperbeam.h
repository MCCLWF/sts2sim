// Models/Cards/Hyperbeam.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Hyperbeam : CardModel { SIM_MODEL(Hyperbeam, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play)
            .TargetingAllOpponents(CombatState()).Execute(ctx);
        PowerCmd::Apply(PowerId::HYPERBEAM_FOCUS_DOWN_POWER, ctx, Owner().Creature(),
                        DynamicVars()["FocusPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}
