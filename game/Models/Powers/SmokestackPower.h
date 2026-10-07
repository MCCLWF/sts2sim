#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct SmokestackPower : PowerModel { SIM_MODEL(SmokestackPower, PowerModel)
    void AfterCardGeneratedForCombat(CardModel card, Player creator) const {
        if (!creator || creator.Creature() != Owner() || card.Type() != CardType::Status) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().CombatState().HittableEnemies(), Dec(Amount()), ValueProp::Unpowered, Owner());
    }
};
}
