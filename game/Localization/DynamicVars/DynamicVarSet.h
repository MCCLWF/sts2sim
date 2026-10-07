// Localization/DynamicVars/DynamicVar.cs and DynamicVarSet.cs.
//
// A DynamicVar is addressed by its owner model and its index in the owner's
// table vars (data(id).vars). Storage depends on the owner kind: card vars
// keep the table value for the card's upgrade level unless overridden
// (CardState::var_overrides); power and relic vars live in their state.
#pragma once

#include <cstdint>
#include <string_view>

#include "core/dec.h"
#include "game/Models/AbstractModel.h"

namespace sts2 {

class DynamicVar {
public:
    DynamicVar() = default;
    DynamicVar(AbstractModel owner, int8_t index) : owner_(owner), index_(index) {}

    explicit operator bool() const { return index_ >= 0; }
    std::string_view Name() const;
    Dec BaseValue() const;
    void SetBaseValue(Dec v) const;
    int32_t IntValue() const { return BaseValue().to_int(); }  // (int)BaseValue
    void UpgradeValueBy(Dec addend) const { SetBaseValue(BaseValue() + addend); }
    ValueProp Props() const;  // DamageVar / BlockVar value props
    PowerId Power() const;    // PowerVar<T>
    VarType Type() const;
    bool IsCalculated() const;
    Dec Calculate(Creature target) const;  // shipped CalculatedVar.Calculate
    // Raw value of the card's game formula before Calculate applies its
    // active-combat/attachment guard. This does not select a caller policy.
    Dec RawCalculationMultiplier(Creature target) const;

private:
    friend ExecutionValue FreezeExecutionValue(const DynamicVar&);
    AbstractModel owner_;
    int8_t index_ = -1;
};

class DynamicVarSet {
public:
    DynamicVarSet() = default;
    explicit DynamicVarSet(AbstractModel owner) : owner_(owner) {}

    int32_t Count() const;
    bool ContainsKey(std::string_view key) const;
    DynamicVar operator[](std::string_view key) const;  // GameError when missing

    DynamicVar Block() const { return (*this)["Block"]; }
    DynamicVar CalculatedBlock() const { return (*this)["CalculatedBlock"]; }
    DynamicVar CalculatedDamage() const { return (*this)["CalculatedDamage"]; }
    DynamicVar CalculationBase() const { return (*this)["CalculationBase"]; }
    DynamicVar CalculationExtra() const { return (*this)["CalculationExtra"]; }
    DynamicVar Cards() const { return (*this)["Cards"]; }
    DynamicVar Damage() const { return (*this)["Damage"]; }
    DynamicVar Dexterity() const { return (*this)["DexterityPower"]; }
    DynamicVar Doom() const { return (*this)["DoomPower"]; }
    DynamicVar Energy() const { return (*this)["Energy"]; }
    DynamicVar ExtraDamage() const { return (*this)["ExtraDamage"]; }
    DynamicVar Forge() const { return (*this)["Forge"]; }
    DynamicVar Gold() const { return (*this)["Gold"]; }
    DynamicVar Heal() const { return (*this)["Heal"]; }
    DynamicVar HpLoss() const { return (*this)["HpLoss"]; }
    DynamicVar MaxHp() const { return (*this)["MaxHp"]; }
    DynamicVar OstyDamage() const { return (*this)["OstyDamage"]; }
    DynamicVar Poison() const { return (*this)["PoisonPower"]; }
    DynamicVar Repeat() const { return (*this)["Repeat"]; }
    DynamicVar Stars() const { return (*this)["Stars"]; }
    DynamicVar Strength() const { return (*this)["StrengthPower"]; }
    DynamicVar Summon() const { return (*this)["Summon"]; }
    DynamicVar Vulnerable() const { return (*this)["VulnerablePower"]; }
    DynamicVar Weak() const { return (*this)["WeakPower"]; }

private:
    AbstractModel owner_;
};

}  // namespace sts2
