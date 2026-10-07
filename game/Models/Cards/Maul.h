// Models/Cards/Maul.cs
#pragma once

#include "game/Commands/Builders/AttackCommand.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Maul : CardModel {
    SIM_MODEL(Maul, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .WithHitCount(2)
            .FromCard(*this, play)
            .Targeting(play->Target)
            .Execute(ctx);

        const Dec increase = DynamicVars()["Increase"].BaseValue();
        for (CardModel card : Owner().PlayerCombatState().AllCards()) {
            if (card.CardId_() != CardId::MAUL) continue;
            DynamicVar damage = card.DynamicVars().Damage();
            damage.SetBaseValue(damage.BaseValue() + increase);
            card.st().extra[0] += increase.to_int();
        }
    }

    void AfterDowngraded() const {
        DynamicVars().Damage().SetBaseValue(DynamicVars().Damage().BaseValue() + Dec(extra(0)));
    }
};

}  // namespace sts2
