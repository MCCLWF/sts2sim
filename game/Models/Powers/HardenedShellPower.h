// Models/Powers/HardenedShellPower.cs
#pragma once
#include "game/Models/PowerModel.h"
namespace sts2 {
struct HardenedShellPower : PowerModel {
    SIM_MODEL(HardenedShellPower, PowerModel)
    // No canonical DynamicVars: reserve vars[0] for exact private decimal Data.
    Dec DamageReceivedThisTurn() const { return st().vars[0]; }
    void SetDamageReceivedThisTurn(Dec v) const { st().vars[0] = v; }
    Dec ModifyHpLostBeforeOstyLate(Creature target, Dec amount, ValueProp, Creature, CardModel) const {
        if (target != Owner() || amount == Dec(0)) return amount;
        Dec remaining = Dec(Amount()) - DamageReceivedThisTurn();
        return amount < remaining ? amount : remaining;
    }
    void AfterModifyingHpLostBeforeOsty() const {}
    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result, ValueProp, Creature, CardModel) const {
        if (target != Owner() || result.WasFullyBlocked) return;
        SetDamageReceivedThisTurn(DamageReceivedThisTurn() + Dec(result.UnblockedDamage));
        if (DamageReceivedThisTurn() >= Dec(Amount())) Owner().SetHpDisplay(HpDisplay::InfiniteWithNumbers);
    }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList&, sts2::CombatState) const {
        SetDamageReceivedThisTurn(Dec(0));
        Owner().SetHpDisplay(HpDisplay::Normal);
    }
};
}
