// Models/Cards/Fisticuffs.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "core/csharp_int.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Fisticuffs : CardModel {
    SIM_MODEL(Fisticuffs, CardModel)

    bool GainsBlock() const { return true; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        AttackCommand attack = DamageCmd::Attack(DynamicVars().Damage().BaseValue())
                                   .FromCard(*this, play).Targeting(play->Target).WithHitFx("vfx/vfx_attack_slash")
                                   .Execute(ctx);
        int32_t block = SumDamageResults(attack.Results(), true);
        CreatureCmd::GainBlock(Owner().Creature(), Dec(block), ValueProp::Move, play);
    }
};

}  // namespace sts2
