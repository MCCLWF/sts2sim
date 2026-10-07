// Models/Powers/SuckPower.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Commands/Builders/AttackCommand.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct SuckPower : PowerModel {
    SIM_MODEL(SuckPower, PowerModel)
    void AfterAttack(PlayerChoiceContext& choiceContext, AttackCommand& command) const {
        if (command.Attacker() != Owner() || command.TargetSide() == Owner().Side() || !IsPoweredAttack(command.DamageProps())) return;
        int32_t num = 0;
        for (const auto& results : command.Results()) {
            for (const DamageResult& r : results) {
                if (r.UnblockedDamage > 0) { ++num; break; }
            }
        }
        if (num > 0) PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, Owner(), Dec(Amount() * num), Owner(), CardModel{});
    }
};
}
