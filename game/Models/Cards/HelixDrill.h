#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
#include "core/csharp_int.h"
namespace sts2 {
struct HelixDrill : CardModel { SIM_MODEL(HelixDrill, CardModel)
    int32_t CalculatedHits() const {
        int32_t spent = checked_int32(Owner().Creature().CombatState().st().player_energy_spent_this_turn);
        if (Pile() && Pile().Type() == PileType::Play) spent -= EnergyCost().GetWithModifiers(CostModifiers::All);
        return DynamicVars().CalculationBase().IntValue() + DynamicVars().CalculationExtra().IntValue() * spent;
    }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(CalculatedHits()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};
}
