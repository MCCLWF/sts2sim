#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Claw : CardModel { SIM_MODEL(Claw, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        for (CardModel card : Owner().PlayerCombatState().AllCards()) {
            if (card.CardId_() != CardId::CLAW) continue;
            Dec increase = DynamicVars()["Increase"].BaseValue();
            card.DynamicVars().Damage().UpgradeValueBy(increase);
            card.SetGrowthDamage(card.GrowthDamage() + increase);
        }
    }
    void AfterDowngraded() const {
        DynamicVars().Damage().SetBaseValue(DynamicVars().Damage().BaseValue() + GrowthDamage());
    }
};
}
