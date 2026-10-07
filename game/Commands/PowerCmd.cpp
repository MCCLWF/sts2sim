// Commands/PowerCmd.cs
#include "game/Commands/PowerCmd.h"

#include "engine/sim/alloc.h"
#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Hooks/Hook.h"

namespace sts2::PowerCmd {

StaticVec<PowerModel, kMaxCreatureList> Apply(PowerId power, PlayerChoiceContext& choiceContext, const CreatureList& targets,
                                              Dec amount, Creature applier, CardModel cardSource, bool silent) {
    StaticVec<PowerModel, kMaxCreatureList> powers;
    for (Creature target : targets) {
        PowerModel val = Apply(power, choiceContext, target, amount, applier, cardSource, silent);
        if (val) powers.push_back(val);
    }
    return powers;
}

PowerModel Apply(PowerId powerId, PlayerChoiceContext& choiceContext, Creature target, Dec amount, Creature applier,
                 CardModel cardSource, bool silent) {
    Sim* sim = &target.sim();
    if (CombatManager(sim).IsEnding()) return {};
    if (!target.CanReceivePowers()) return {};
    PowerModel power = FindExistingInstanceForStacking(powerId, target, applier);
    if (!power) {
        power = PowerModel(sim, alloc_power(*sim->g, powerId));  // ModelDb.Power<T>().ToMutable()
        Apply(choiceContext, power, target, amount, applier, cardSource, silent);
    } else if (ModifyAmount(choiceContext, power, amount, applier, cardSource, silent) == 0) {
        power = {};
    }
    return power;
}

void Apply(PlayerChoiceContext& choiceContext, PowerModel power, Creature target, Dec amount, Creature applier,
           CardModel cardSource, bool silent) {
    Sim* sim = &target.sim();
    CombatManager cm(sim);
    if (cm.IsEnding() || amount == Dec(0) || !target.CanReceivePowers()) return;
    CombatState combatState = target.CombatState();
    if (!combatState) return;
    PowerModel existing = FindExistingInstanceForStacking(power.PowerId_(), target, applier);
    if (existing) {
        ModifyAmount(choiceContext, existing, amount, applier, cardSource);
        return;
    }
    power.SetApplier(applier);
    Hook::BeforePowerAmountChanged(combatState, power, amount, target, applier, cardSource);
    Dec modifiedAmount = amount;
    ModelList givenModifiers;
    bool hasGiven = false;
    if (applier && combatState.ContainsCreature(applier)) {
        modifiedAmount = Hook::ModifyPowerAmountGiven(combatState, power, applier, modifiedAmount, target, cardSource, givenModifiers);
        hasGiven = true;
    }
    ModelList receivedModifiers;
    modifiedAmount = Hook::ModifyPowerAmountReceived(combatState, power, target, modifiedAmount, applier, receivedModifiers);
    if (combatState.Players().size() > 1 && (target.IsPrimaryEnemy() || target.IsSecondaryEnemy()) && power.ShouldScaleInMultiplayer()) {
        modifiedAmount = power.GetScaledAmountForMultiplayer(combatState, applier, modifiedAmount, target, cardSource);
    }
    power.BeforeApplied(target, modifiedAmount, applier, cardSource);
    if (target.CanReceivePowers()) {
        power.ApplyInternal(target, modifiedAmount, silent);
        if (modifiedAmount != Dec(0)) cm.History().PowerReceived(combatState, power, modifiedAmount, applier);
        if (target.Side() == CombatSide::Player && power.Type() == PowerType::Debuff) power.SetSkipNextDurationTick(true);
        if (hasGiven) Hook::AfterModifyingPowerAmountGiven(combatState, givenModifiers, power);
        Hook::AfterModifyingPowerAmountReceived(combatState, receivedModifiers, power);
        if (modifiedAmount != Dec(0)) {
            power.AfterApplied(applier, cardSource);
            Hook::AfterPowerAmountChanged(combatState, choiceContext, power, modifiedAmount, applier, cardSource);
        }
    }
}

PowerModel FindExistingInstanceForStacking(PowerId basePower, Creature target, Creature applier) {
    switch (data(basePower).instance_type) {
    case PowerInstanceType::Instanced: return {};
    case PowerInstanceType::InstancedPerApplier:
        for (PowerModel p : target.GetPowerInstances(basePower)) {
            if (p.Applier() == applier) return p;
        }
        return {};
    case PowerInstanceType::None: return target.GetPower(basePower);
    }
    throw GameError("FindExistingInstanceForStacking: unknown InstanceType");
}

void Decrement(PowerModel power) {
    PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
    ModifyAmount(ctx, power, Dec(-1), Creature{}, CardModel{});
}

void TickDownDuration(PowerModel power) {
    if (power.SkipNextDurationTick()) {
        power.SetSkipNextDurationTick(false);
    } else {
        Decrement(power);
    }
}

int32_t ModifyAmount(PlayerChoiceContext& choiceContext, PowerModel power, Dec offset, Creature applier, CardModel cardSource,
                     bool silent) {
    Sim* sim = &power.sim();
    CombatManager cm(sim);
    if (cm.IsEnding()) return 0;
    Creature owner = power.Owner();
    CombatState combatState = owner.CombatState();
    if (!combatState) return 0;
    Hook::BeforePowerAmountChanged(combatState, power, offset, owner, applier, cardSource);
    Dec modifiedOffset = offset;
    ModelList modifiers;
    bool hasGiven = false;
    if (applier && combatState.ContainsCreature(applier)) {
        modifiedOffset = Hook::ModifyPowerAmountGiven(combatState, power, applier, modifiedOffset, owner, cardSource, modifiers);
        hasGiven = true;
    }
    ModelList receivedModifiers;
    modifiedOffset = Hook::ModifyPowerAmountReceived(combatState, power, owner, modifiedOffset, applier, receivedModifiers);
    cm.History().PowerReceived(combatState, power, modifiedOffset, applier);
    int32_t newAmount = power.Amount() + modifiedOffset.to_int();
    power.SetAmount(newAmount, silent);
    if (hasGiven) Hook::AfterModifyingPowerAmountGiven(combatState, modifiers, power);
    Hook::AfterModifyingPowerAmountReceived(combatState, receivedModifiers, power);
    if (modifiedOffset.to_int() != 0) {
        Hook::AfterPowerAmountChanged(combatState, choiceContext, power, modifiedOffset, applier, cardSource);
    }
    if (power.ShouldRemoveDueToAmount()) Remove(power);
    return newAmount;
}

void Remove(PowerId power, Creature creature) { Remove(creature.GetPower(power)); }

void Remove(PowerModel power) {
    if (!power) return;
    power.RemoveInternal();
    power.AfterRemoved(power.Owner());
}

}  // namespace sts2::PowerCmd
