// Models/Powers/VigorPower.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Commands/Builders/AttackCommand.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"
namespace sts2 {
struct VigorPower : PowerModel {
    SIM_MODEL(VigorPower, PowerModel)
    bool Active() const { return extra(0) != 0; }
    void SetActive(bool v) const { extra(0) = v; }
    int32_t AmountWhenAttackStarted() const { return extra(1); }
    void SetAmountWhenAttackStarted(int32_t v) const { extra(1) = v; }
    void BeforeAttack(AttackCommand& command) const {
        if (command.Attacker() != Owner() || !IsPoweredAttack(command.DamageProps()) || Active()) return;
        SetActive(true); SetAmountWhenAttackStarted(Amount());
    }
    Dec ModifyDamageAdditive(Creature, Dec amount, ValueProp props, Creature dealer, CardModel, const CardPlay*) const {
        if (Owner() != dealer || !IsPoweredAttack(props)) return Dec(0);
        return Dec(Amount());
    }
    void AfterAttack(PlayerChoiceContext& ctx, AttackCommand& command) const {
        if (!Active() || command.Attacker() != Owner()) return;
        PowerCmd::ModifyAmount(ctx, *this, Dec(-AmountWhenAttackStarted()), Creature{}, CardModel{});
        SetActive(false); SetAmountWhenAttackStarted(0);
    }
};
}
