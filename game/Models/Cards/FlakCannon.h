#pragma once
#include "game/Commands/CardCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct FlakCannon : CardModel { SIM_MODEL(FlakCannon, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CardList statuses;
        for (CardModel card : Owner().PlayerCombatState().AllCards())
            if (card.Type() == CardType::Status && card.Pile().Type() != PileType::Exhaust) statuses.push_back(card);
        int32_t hits = static_cast<int32_t>(statuses.size());
        for (CardModel card : statuses) CardCmd::Exhaust(ctx, card);
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(hits).FromCard(*this, play)
            .TargetingRandomOpponents(CombatState()).Execute(ctx);
    }
};
}
