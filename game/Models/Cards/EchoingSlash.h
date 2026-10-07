#pragma once

#include "game/Commands/Builders/AttackContext.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct EchoingSlash : CardModel {
    SIM_MODEL(EchoingSlash, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        AttackContext attack(CombatState(), ctx, *this, play);
        attack.Run([&] {
            int32_t attackCount = 1;
            while (attackCount > 0) {
                --attackCount;
                DamageResults results = CreatureCmd::Damage(ctx, CombatState().HittableEnemies(), DynamicVars().Damage(),
                                                            Owner().Creature(), *this, play);
                attack.AddHit(results);
                for (const DamageResult& result : results) {
                    if (result.WasTargetKilled) ++attackCount;
                }
            }
        });
    }
};

}  // namespace sts2
