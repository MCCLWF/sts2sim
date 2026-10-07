#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct HellraiserPower : PowerModel {
    SIM_MODEL(HellraiserPower, PowerModel)

    // The game implementation only changes attack visuals here.
    void BeforeAttack(AttackCommand&) const {}

    void AfterCardDrawnEarly(PlayerChoiceContext& ctx, CardModel card, bool) const {
        if (!card || card.Owner().Creature() != Owner() || !card.HasTag(CardTag::Strike)) return;
        bool play = true;
        bool allInfinite = true;
        for (Creature enemy : Owner().CombatState().HittableEnemies())
            if (enemy.HpDisplay() != HpDisplay::InfiniteWithNumbers && enemy.HpDisplay() != HpDisplay::InfiniteWithoutNumbers)
                allInfinite = false;
        if (allInfinite) {
            if (extra(0) >= 9) play = false;
            // The native counter advances even after the cap is reached.
            extra(0) = static_cast<int32_t>(static_cast<uint32_t>(extra(0)) + 1U);
        } else {
            extra(0) = 0;
        }
        if (play) CardCmd::AutoPlay(ctx, card, Creature{});
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) extra(0) = 0;
    }
};

}  // namespace sts2
