// Models/Powers/PlatingPower.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct PlatingPower : PowerModel {
    SIM_MODEL(PlatingPower, PowerModel)
    Dec GetScaledAmountForMultiplayer(sts2::CombatState, sts2::Creature, Dec amount, sts2::Creature, sts2::CardModel) const { return amount; }
    void AfterApplied(Creature, CardModel) const {
        if (Owner().IsEnemy() && DynamicVars().ContainsKey("Decrement")) DynamicVars()["Decrement"].SetBaseValue(Dec(1));
    }
    void BeforeSideTurnStart(PlayerChoiceContext& choiceContext, CombatSide side, const CreatureList&, sts2::CombatState combatState) const {
        if (side != CombatSide::Player || Owner().IsPlayer() || combatState.RoundNumber() > 1) return;
        CreatureCmd::GainBlock(Owner(), Dec(Amount()), ValueProp::Unpowered, nullptr);
    }
    void BeforeSideTurnEndEarly(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        for (Creature c : participants) if (c == Owner()) {
            CreatureCmd::GainBlock(Owner(), Dec(Amount()), ValueProp::Unpowered, nullptr);
            break;
        }
    }
    void AfterSideTurnStart(CombatSide side, const CreatureList& participants, sts2::CombatState combatState) const {
        bool present = false; for (Creature c : participants) if (c == Owner()) present = true;
        if (!present) return;
        if (Owner().IsEnemy()) {
            if (combatState.RoundNumber() == 1) return;
            PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
            Dec decrement = DynamicVars().ContainsKey("Decrement") ? DynamicVars()["Decrement"].BaseValue() : Dec(1);
            PowerCmd::ModifyAmount(ctx, *this, Dec(0) - decrement, Creature{}, CardModel{});
        } else {
            if (Owner().Player() && Owner().Player().PlayerCombatState().TurnNumber() == 1) return;
            PowerCmd::Decrement(*this);
        }
    }
};
}
