// Models/Cards/KinglyPunch.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct KinglyPunch : CardModel {
    SIM_MODEL(KinglyPunch, CardModel)

    int32_t ExtraDamage() const { return extra(0); }
    void SetExtraDamage(int32_t amount) const { extra(0) = amount; }

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (!cardPlay || !cardPlay->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .FromCard(*this, cardPlay)
            .Targeting(cardPlay->Target)
            .Execute(choiceContext);
    }
    void AfterCardDrawn(PlayerChoiceContext&, CardModel card, bool) const {
        if (card != *this) return;
        DynamicVar damage = DynamicVars().Damage();
        int32_t increase = DynamicVars()["Increase"].IntValue();
        damage.SetBaseValue(damage.BaseValue() + Dec(increase));
        SetExtraDamage(ExtraDamage() + increase);
    }
    void AfterDowngraded() const {
        DynamicVar damage = DynamicVars().Damage();
        damage.SetBaseValue(damage.BaseValue() + Dec(ExtraDamage()));
    }
};

}  // namespace sts2
