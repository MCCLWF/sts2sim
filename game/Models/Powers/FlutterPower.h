// Models/Powers/FlutterPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {
struct FlutterPower : PowerModel {
    SIM_MODEL(FlutterPower, PowerModel)

    Dec ModifyDamageMultiplicative(Creature target, Dec, ValueProp props, Creature, CardModel, const CardPlay*) const {
        if (target != Owner() || !IsPoweredAttack(props)) return Dec(1);
        return DynamicVars()["DamageDecrease"].BaseValue() / Dec(100);
    }
    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result, ValueProp props,
                             Creature, CardModel) const {
        if (target != Owner() || result.UnblockedDamage == 0 || !IsPoweredAttack(props)) return;
        PowerCmd::Decrement(*this);
        if (Amount() <= 0) {
            MonsterModel monster = Owner().Monster();
            const auto& machine = monster.MachineDef();
            const int8_t last = machine.Find(monster.LastLoggedMoveId());
            // Source MoveState.GetNextState reads its follow-up without drawing RNG.
            const int8_t overrideNext = monster.st().machine.follow_up[static_cast<size_t>(last)];
            const int8_t next = overrideNext >= 0 ? overrideNext : machine.states[static_cast<size_t>(last)].followUp;
            CreatureCmd::Stun(Owner(), STS2_NATIVE_CALLBACK(Move, FlutterPower,
                ([](MonsterModel, const CreatureList&) {}), "stunned_noop"), machine.states[static_cast<size_t>(next)].id);
            monster.st().extra[0] = 0;  // ThievingHopper.IsHovering
        }
    }
};
}  // namespace sts2
