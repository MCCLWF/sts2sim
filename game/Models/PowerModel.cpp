// Models/PowerModel.cs
#include "game/Models/PowerModel.h"

#include <algorithm>

#include "game/All.h"

namespace sts2 {

// The computed Type of the TemporaryStrength/Dexterity/FocusPower subclasses
// (!IsPositive ? Debuff : Buff) is a constant per class (IsPositive is), so
// the table value is right.
PowerType PowerModel::Type() const { return data().type; }

// StackType overrides that are not constants.
PowerStackType PowerModel::StackType() const {
    switch (PowerId_()) {
    case PowerId::SHRINK_POWER:  // IsInfinite => Amount < 0
        return Amount() >= 0 ? PowerStackType::Counter : PowerStackType::Single;
    case PowerId::MONOLOGUE_POWER:
        return DynamicVars()["StrengthApplied"].IntValue() != 0 ? PowerStackType::Counter : PowerStackType::None;
    default: return data().stack_type;
    }
}

sts2::Creature PowerModel::Owner() const { return sts2::Creature(sim_, st().owner); }
sts2::Creature PowerModel::Applier() const { return sts2::Creature(sim_, st().applier); }
void PowerModel::SetApplier(sts2::Creature c) const { st().applier = c.Index(); }
sts2::Creature PowerModel::Target() const { return sts2::Creature(sim_, st().target); }
void PowerModel::SetTarget(sts2::Creature c) const { st().target = c.Index(); }
sts2::CombatState PowerModel::CombatState() const { return Owner().CombatState(); }

PowerType PowerModel::GetTypeForAmount(Dec customAmount) const {
    if (StackType() == PowerStackType::Counter && AllowNegative() && customAmount < Dec(0)) return PowerType::Debuff;
    if (!AllowNegative() && Type() == PowerType::Debuff && customAmount < Dec(0)) return PowerType::Buff;
    return Type();
}

bool PowerModel::ShouldRemoveDueToAmount() const {
    if (AllowNegative() || Amount() > 0) {
        if (AllowNegative()) return Amount() == 0;
        return false;
    }
    return true;
}

void PowerModel::SetAmount(int32_t amount, bool silent) const {
    amount = std::clamp(amount, -999999999, 999999999);
    int32_t num = amount - st().amount;
    if (num != 0) {
        if (sim_->execution_trace)
            sim_->execution_trace->RecordPower({ExecutionPowerEventKind::AmountChanged,
                Index(), st().owner, st().owner ? static_cast<uint32_t>(Owner().CombatId()) : UINT32_MAX, static_cast<uint16_t>(PowerId_()),
                st().amount, amount, st().owner && Owner().st().powers.contains(Index())});
        st().amount = amount;
        Owner().InvokePowerModified(*this, num, silent);
    }
}

void PowerModel::ApplyInternal(sts2::Creature owner, Dec amount, bool silent) const {
    if (amount == Dec(0)) return;
    if (st().owner != 0 && st().owner != owner.Index()) throw GameError("Cannot move a power from one owner to another");
    st().owner = owner.Index();
    SetAmount(amount.to_int(), silent);
    owner.ApplyPowerInternal(*this);
}

void PowerModel::RemoveInternal() const {
    sts2::CombatState combatState(sim_);
    if (combatState) combatState.ForgetCardPlayPowerCaptures(*this);
    Owner().RemovePowerInternal(*this);
}

Dec PowerModel::GetScaledAmountForMultiplayer_Default(sts2::CombatState combatState, sts2::Creature applier, Dec amount,
                                                      sts2::Creature target, CardModel cardSource) const {
    throw GameError("multiplayer scaling is not supported (single player only)");
}

}  // namespace sts2
