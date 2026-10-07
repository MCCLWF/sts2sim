// Models/Powers/SkittishPower.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/Builders/AttackCommand.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct SkittishPower : PowerModel {
    SIM_MODEL(SkittishPower, PowerModel)
    bool HasGainedBlockThisTurn() const { return extra(0) != 0; }
    void SetHasGainedBlockThisTurn(bool v) const { extra(0) = v; }
    void AfterAttack(PlayerChoiceContext&, AttackCommand& command) const {
        if (HasGainedBlockThisTurn() || !has_flag(command.DamageProps(), ValueProp::Move) || command.ModelSource().Kind() != ModelKind::Card) return;
        for (const auto& rs : command.Results()) for (const DamageResult& r : rs) if (r.Receiver == Owner() && r.UnblockedDamage != 0) {
            SetHasGainedBlockThisTurn(true); CreatureCmd::GainBlock(Owner(), Dec(Amount()), ValueProp::Unpowered, nullptr); return;
        }
    }
    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide side, const CreatureList&) const { if (side != Owner().Side() && HasGainedBlockThisTurn()) SetHasGainedBlockThisTurn(false); }
    Dec GetScaledAmountForMultiplayer(sts2::CombatState, sts2::Creature, Dec amount, sts2::Creature, sts2::CardModel) const { return amount; }
};
}
