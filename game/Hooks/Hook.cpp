// Hooks/Hook.cs
//
// Every dispatcher iterates hook listeners the way the C# enumerables do:
// the listener list is a snapshot taken when enumeration starts, and each
// item is checked with Contains() only when the enumeration reaches it
// (docs/design-engine.md §7). IterateCombatHookListeners additionally yields
// nothing when combat is over or ending (checked once, at the start).
#include "game/Hooks/Hook.h"
#include "game/Hooks/HookExecutionScope.h"

#include "game/Combat/CombatManager.h"
#include "game/Models/Relics/TheCourier.h"

namespace sts2 {

// A lazily checked listener enumeration. `run` part: RunState.IterateHookListeners
// (the run-level list, then the child combat's listeners, whose snapshot is
// taken only when the enumeration gets there). `combat` part only:
// CombatState.IterateHookListeners.
class HookListenerRange {
public:
    static HookListenerRange Combat(CombatState combat) {
        HookListenerRange r;
        r.combat_ = combat;
        r.phase_ = 1;
        return r;
    }
    static HookListenerRange Run(RunState run, CombatState childCombat) {
        HookListenerRange r;
        r.run_ = run;
        r.combat_ = childCombat;
        r.phase_ = 0;
        return r;
    }
    static HookListenerRange Empty() {
        HookListenerRange r;
        r.phase_ = 2;
        return r;
    }

    class iterator {
    public:
        explicit iterator(HookListenerRange* r) : r_(r) {
            if (r_ != nullptr && !r_->Advance()) r_ = nullptr;
        }
        AbstractModel operator*() const { return r_->current_; }
        iterator& operator++() {
            if (!r_->Advance()) r_ = nullptr;
            return *this;
        }
        bool operator!=(const iterator& o) const { return r_ != o.r_; }

    private:
        HookListenerRange* r_;
    };
    iterator begin() { return iterator(started_ ? nullptr : (started_ = true, this)); }
    iterator end() { return iterator(nullptr); }

private:
    // Moves to the next listener that is still in the state; false at the end.
    bool Advance() {
        for (;;) {
            if (phase_ == 0) {
                if (!listed_) {
                    list_ = run_.IterateHookListeners(combat_);  // the run-level part only
                    listed_ = true;
                    i_ = 0;
                }
                while (i_ < list_.size()) {
                    AbstractModel m = list_[i_++];
                    if (run_.Contains(m)) {
                        current_ = m;
                        return true;
                    }
                }
                if (!combat_) return false;  // childCombatState == null: yield break
                phase_ = 1;
                listed_ = false;
            }
            if (phase_ == 1) {
                if (!listed_) {
                    list_ = combat_ ? combat_.IterateHookListeners() : HookListenerList{};
                    listed_ = true;
                    i_ = 0;
                }
                while (i_ < list_.size()) {
                    AbstractModel m = list_[i_++];
                    if (combat_.Contains(m)) {
                        current_ = m;
                        return true;
                    }
                }
                return false;
            }
            return false;
        }
    }

    RunState run_;
    CombatState combat_;
    int phase_ = 2;
    bool listed_ = false;
    bool started_ = false;
    size_t i_ = 0;
    HookListenerList list_;
    AbstractModel current_;
};

namespace Hook {

namespace {

// Hook.IterateCombatHookListeners
HookListenerRange IterateCombatHookListeners(CombatState combatState) {
    CombatManager cm(&combatState.sim());
    if (cm.IsOverOrEnding() && !cm.IsStarting()) return HookListenerRange::Empty();
    return HookListenerRange::Combat(combatState);
}

// combatState.IterateHookListeners(): no combat-end suppression.
HookListenerRange IterateHookListenersOf(CombatState combatState) { return HookListenerRange::Combat(combatState); }

// runState.IterateHookListeners(combatState)
HookListenerRange IterateRunHookListeners(RunState runState, CombatState combatState) {
    return HookListenerRange::Run(runState, combatState);
}

// LINQ Contains over a listener enumeration: walks it (with its Contains checks).
bool range_contains(HookListenerRange range, AbstractModel m) {
    for (AbstractModel x : range) {
        if (x == m) return true;
    }
    return false;
}

bool list_contains(const ModelList& list, AbstractModel m) {
    for (const AbstractModel& x : list) {
        if (x == m) return true;
    }
    return false;
}

// Hook.ModifyDamageInternal
Dec ModifyDamageInternal(RunState runState, CombatState combatState, Creature target, Creature dealer, Dec damage,
                         ValueProp props, CardModel cardSource, const CardPlay* cardPlay,
                         ModifyDamageHookType modifyDamageHookType, ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyDamageInternal", runState, combatState, target, dealer, damage, props, cardSource, cardPlay, modifyDamageHookType, modifiers);
    return hook_execution_observation.Invoke("Decimal", [&]() -> Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = damage;
    ModelList list;
    if (has_flag(modifyDamageHookType, ModifyDamageHookType::Additive)) {
        for (AbstractModel item : IterateRunHookListeners(runState, combatState)) {
            Dec num2 = item.ModifyDamageAdditive(target, num, props, dealer, cardSource, cardPlay);
            num = num + num2;
            if (num2 != Dec(0)) list.push_back(item);
        }
    }
    if (has_flag(modifyDamageHookType, ModifyDamageHookType::Multiplicative)) {
        for (AbstractModel item2 : IterateRunHookListeners(runState, combatState)) {
            Dec num3 = item2.ModifyDamageMultiplicative(target, num, props, dealer, cardSource, cardPlay);
            num = num * num3;
            if (num3 != Dec(1)) list.push_back(item2);
        }
    }
    if (has_flag(modifyDamageHookType, ModifyDamageHookType::Cap)) {
        Dec num4 = Dec::max_value();
        for (AbstractModel item3 : IterateRunHookListeners(runState, combatState)) {
            Dec num5 = item3.ModifyDamageCap(target, props, dealer, cardSource, cardPlay);
            if (num5 < num4) {
                num4 = num5;
                if (num > num5) {
                    num = num5;
                    list.push_back(item3);
                }
            }
        }
    }
    modifiers = list;
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"damage", "Decimal", 0, FreezeExecutionValue(damage)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}, ExecutionArgument{"modifyDamageHookType", "ModifyDamageHookType", 0, FreezeExecutionValue(modifyDamageHookType)}, ExecutionArgument{"modifiers", "List<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"damage", "Decimal", 0, FreezeExecutionValue(damage)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}, ExecutionArgument{"modifyDamageHookType", "ModifyDamageHookType", 0, FreezeExecutionValue(modifyDamageHookType)}, ExecutionArgument{"modifiers", "List<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

}  // namespace


// Hooks/Hook.cs: AfterActEntered
void AfterActEntered(sts2::RunState runState) {
    HookExecutionScope hook_execution_observation("Hook.AfterActEntered", runState);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, CombatState())) {
        model.AfterActEntered();
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}}; });
}

// Hooks/Hook.cs: BeforeAttack
void BeforeAttack(sts2::CombatState combatState, sts2::AttackCommand& command) {
    HookExecutionScope hook_execution_observation("Hook.BeforeAttack", combatState, command);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.BeforeAttack(command);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"command", "AttackCommand", 0, FreezeExecutionValue(command)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"command", "AttackCommand", 0, FreezeExecutionValue(command)}}; });
}

// Hooks/Hook.cs: AfterAttack
void AfterAttack(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::AttackCommand& command) {
    HookExecutionScope hook_execution_observation("Hook.AfterAttack", combatState, choiceContext, command);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterAttack(choiceContext, command);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"command", "AttackCommand", 0, FreezeExecutionValue(command)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"command", "AttackCommand", 0, FreezeExecutionValue(command)}}; });
}

// Hooks/Hook.cs: AfterBlockBroken
void AfterBlockBroken(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Creature target, sts2::Creature breaker) {
    HookExecutionScope hook_execution_observation("Hook.AfterBlockBroken", combatState, choiceContext, target, breaker);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateHookListenersOf(combatState)) {
        model.AfterBlockBroken(choiceContext, target, breaker);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"breaker", "Creature", 0, FreezeExecutionValue(breaker)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"breaker", "Creature", 0, FreezeExecutionValue(breaker)}}; });
}

// Hooks/Hook.cs: AfterBlockCleared
void AfterBlockCleared(sts2::CombatState combatState, sts2::Creature creature) {
    HookExecutionScope hook_execution_observation("Hook.AfterBlockCleared", combatState, creature);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterBlockCleared(creature);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; });
}

// Hooks/Hook.cs: BeforeBlockGained
void BeforeBlockGained(sts2::CombatState combatState, sts2::Creature creature, sts2::Dec amount, sts2::ValueProp props, sts2::CardModel cardSource) {
    HookExecutionScope hook_execution_observation("Hook.BeforeBlockGained", combatState, creature, amount, props, cardSource);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.BeforeBlockGained(creature, amount, props, cardSource);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; });
}

// Hooks/Hook.cs: AfterBlockGained
void AfterBlockGained(sts2::CombatState combatState, sts2::Creature creature, sts2::Dec amount, sts2::ValueProp props, sts2::CardModel cardSource) {
    HookExecutionScope hook_execution_observation("Hook.AfterBlockGained", combatState, creature, amount, props, cardSource);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterBlockGained(creature, amount, props, cardSource);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; });
}

// Hooks/Hook.cs: BeforeCardAutoPlayed
void BeforeCardAutoPlayed(sts2::CombatState combatState, sts2::CardModel card, sts2::Creature target, sts2::AutoPlayType type) {
    HookExecutionScope hook_execution_observation("Hook.BeforeCardAutoPlayed", combatState, card, target, type);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.BeforeCardAutoPlayed(card, target, type);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"type", "AutoPlayType", 0, FreezeExecutionValue(type)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"type", "AutoPlayType", 0, FreezeExecutionValue(type)}}; });
}

// Hooks/Hook.cs: AfterCardChangedPiles
void AfterCardChangedPiles(sts2::RunState runState, sts2::CombatState combatState, sts2::CardModel card, sts2::PileType oldPile, sts2::AbstractModel clonedBy) {
    HookExecutionScope hook_execution_observation("Hook.AfterCardChangedPiles", runState, combatState, card, oldPile, clonedBy);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.AfterCardChangedPiles(card, oldPile, clonedBy);
    }
    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.AfterCardChangedPilesLate(card, oldPile, clonedBy);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"oldPile", "PileType", 0, FreezeExecutionValue(oldPile)}, ExecutionArgument{"clonedBy", "AbstractModel", 0, FreezeExecutionValue(clonedBy)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"oldPile", "PileType", 0, FreezeExecutionValue(oldPile)}, ExecutionArgument{"clonedBy", "AbstractModel", 0, FreezeExecutionValue(clonedBy)}}; });
}

// Hooks/Hook.cs: AfterCardDiscarded
void AfterCardDiscarded(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::CardModel card) {
    HookExecutionScope hook_execution_observation("Hook.AfterCardDiscarded", combatState, choiceContext, card);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterCardDiscarded(choiceContext, card);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}}; });
}

// Hooks/Hook.cs: AfterCardDrawn
void AfterCardDrawn(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::CardModel card, bool fromHandDraw) {
    HookExecutionScope hook_execution_observation("Hook.AfterCardDrawn", combatState, choiceContext, card, fromHandDraw);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterCardDrawnEarly(choiceContext, card, fromHandDraw);
        choiceContext.PopModel(model);
    }
    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterCardDrawn(choiceContext, card, fromHandDraw);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"fromHandDraw", "Boolean", 0, FreezeExecutionValue(fromHandDraw)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"fromHandDraw", "Boolean", 0, FreezeExecutionValue(fromHandDraw)}}; });
}

// Hooks/Hook.cs: AfterCardEnteredCombat
void AfterCardEnteredCombat(sts2::CombatState combatState, sts2::CardModel card) {
    HookExecutionScope hook_execution_observation("Hook.AfterCardEnteredCombat", combatState, card);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterCardEnteredCombat(card);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}}; });
}

// Hooks/Hook.cs: AfterCardExhausted
void AfterCardExhausted(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::CardModel card, bool causedByEthereal) {
    HookExecutionScope hook_execution_observation("Hook.AfterCardExhausted", combatState, choiceContext, card, causedByEthereal);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterCardExhausted(choiceContext, card, causedByEthereal);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"causedByEthereal", "Boolean", 0, FreezeExecutionValue(causedByEthereal)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"causedByEthereal", "Boolean", 0, FreezeExecutionValue(causedByEthereal)}}; });
}

// Hooks/Hook.cs: AfterCardGeneratedForCombat
void AfterCardGeneratedForCombat(sts2::CombatState combatState, sts2::CardModel card, sts2::Player creator) {
    HookExecutionScope hook_execution_observation("Hook.AfterCardGeneratedForCombat", combatState, card, creator);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterCardGeneratedForCombat(card, creator);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"creator", "Player", 0, FreezeExecutionValue(creator)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"creator", "Player", 0, FreezeExecutionValue(creator)}}; });
}

// Hooks/Hook.cs: BeforeCardPlayed
void BeforeCardPlayed(sts2::CombatState combatState, const sts2::CardPlay* cardPlay) {
    HookExecutionScope hook_execution_observation("Hook.BeforeCardPlayed", combatState, cardPlay);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.BeforeCardPlayed(cardPlay);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}}; });
}

// Hooks/Hook.cs: AfterCardPlayed
void AfterCardPlayed(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, const sts2::CardPlay* cardPlay) {
    HookExecutionScope hook_execution_observation("Hook.AfterCardPlayed", combatState, choiceContext, cardPlay);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateHookListenersOf(combatState)) {
        choiceContext.PushModel(model);
        model.AfterCardPlayed(choiceContext, cardPlay);
        choiceContext.PopModel(model);
    }
    for (AbstractModel model : IterateHookListenersOf(combatState)) {
        choiceContext.PushModel(model);
        model.AfterCardPlayedLate(choiceContext, cardPlay);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}}; });
}

// Hooks/Hook.cs: BeforeCardRemoved
void BeforeCardRemoved(sts2::RunState runState, sts2::CardModel card) {
    HookExecutionScope hook_execution_observation("Hook.BeforeCardRemoved", runState, card);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, CombatState())) {
        model.BeforeCardRemoved(card);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}}; });
}

// Hooks/Hook.cs: BeforeCombatStart
void BeforeCombatStart(sts2::RunState runState, sts2::CombatState combatState) {
    HookExecutionScope hook_execution_observation("Hook.BeforeCombatStart", runState, combatState);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.BeforeCombatStart();
    }
    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.BeforeCombatStartLate();
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}}; });
}

// Hooks/Hook.cs: AfterCombatEnd
void AfterCombatEnd(sts2::RunState runState, sts2::CombatState combatState, sts2::CombatRoom room) {
    HookExecutionScope hook_execution_observation("Hook.AfterCombatEnd", runState, combatState, room);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.AfterCombatEnd(room);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"room", "CombatRoom", 0, FreezeExecutionValue(room)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"room", "CombatRoom", 0, FreezeExecutionValue(room)}}; });
}

// Hooks/Hook.cs: AfterCombatVictory
void AfterCombatVictory(sts2::RunState runState, sts2::CombatState combatState, sts2::CombatRoom room) {
    HookExecutionScope hook_execution_observation("Hook.AfterCombatVictory", runState, combatState, room);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.AfterCombatVictoryEarly(room);
    }
    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.AfterCombatVictory(room);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"room", "CombatRoom", 0, FreezeExecutionValue(room)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"room", "CombatRoom", 0, FreezeExecutionValue(room)}}; });
}

// Hooks/Hook.cs: AfterCreatureAddedToCombat
void AfterCreatureAddedToCombat(sts2::CombatState combatState, sts2::Creature creature) {
    HookExecutionScope hook_execution_observation("Hook.AfterCreatureAddedToCombat", combatState, creature);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateHookListenersOf(combatState)) {
        model.AfterCreatureAddedToCombat(creature);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; });
}

// Hooks/Hook.cs: AfterCurrentHpChanged
void AfterCurrentHpChanged(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature creature, sts2::Dec delta) {
    HookExecutionScope hook_execution_observation("Hook.AfterCurrentHpChanged", runState, combatState, creature, delta);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.AfterCurrentHpChanged(creature, delta);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"delta", "Decimal", 0, FreezeExecutionValue(delta)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"delta", "Decimal", 0, FreezeExecutionValue(delta)}}; });
}

// Hooks/Hook.cs: AfterDamageGiven
void AfterDamageGiven(sts2::PlayerChoiceContext& choiceContext, sts2::CombatState combatState, sts2::Creature dealer, const sts2::DamageResult& results, sts2::ValueProp props, sts2::Creature target, sts2::CardModel cardSource) {
    HookExecutionScope hook_execution_observation("Hook.AfterDamageGiven", choiceContext, combatState, dealer, results, props, target, cardSource);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateHookListenersOf(combatState)) {
        choiceContext.PushModel(model);
        model.AfterDamageGiven(choiceContext, dealer, results, props, target, cardSource);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"results", "DamageResult", 0, FreezeExecutionValue(results)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"results", "DamageResult", 0, FreezeExecutionValue(results)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; });
}

// Hooks/Hook.cs: BeforeDamageReceived
void BeforeDamageReceived(sts2::PlayerChoiceContext& choiceContext, sts2::RunState runState, sts2::CombatState combatState, sts2::Creature target, sts2::Dec amount, sts2::ValueProp props, sts2::Creature dealer, sts2::CardModel cardSource) {
    HookExecutionScope hook_execution_observation("Hook.BeforeDamageReceived", choiceContext, runState, combatState, target, amount, props, dealer, cardSource);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        choiceContext.PushModel(model);
        model.BeforeDamageReceived(choiceContext, target, amount, props, dealer, cardSource);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; });
}

// Hooks/Hook.cs: AfterDamageReceived
void AfterDamageReceived(sts2::PlayerChoiceContext& choiceContext, sts2::RunState runState, sts2::CombatState combatState, sts2::Creature target, const sts2::DamageResult& result, sts2::ValueProp props, sts2::Creature dealer, sts2::CardModel cardSource) {
    HookExecutionScope hook_execution_observation("Hook.AfterDamageReceived", choiceContext, runState, combatState, target, result, props, dealer, cardSource);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        choiceContext.PushModel(model);
        model.AfterDamageReceived(choiceContext, target, result, props, dealer, cardSource);
        choiceContext.PopModel(model);
    }
    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        choiceContext.PushModel(model);
        model.AfterDamageReceivedLate(choiceContext, target, result, props, dealer, cardSource);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"result", "DamageResult", 0, FreezeExecutionValue(result)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"result", "DamageResult", 0, FreezeExecutionValue(result)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; });
}

// Hooks/Hook.cs: BeforeDeath
void BeforeDeath(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature creature) {
    HookExecutionScope hook_execution_observation("Hook.BeforeDeath", runState, combatState, creature);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.BeforeDeath(creature);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; });
}

// Hooks/Hook.cs: AfterDeath
void AfterDeath(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature creature, bool wasRemovalPrevented, float deathAnimLength) {
    HookExecutionScope hook_execution_observation("Hook.AfterDeath", runState, combatState, creature, wasRemovalPrevented, deathAnimLength);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        PlayerChoiceContext hookPlayerChoiceContext;
        model.AfterDeath(hookPlayerChoiceContext, creature, wasRemovalPrevented, deathAnimLength);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"wasRemovalPrevented", "Boolean", 0, FreezeExecutionValue(wasRemovalPrevented)}, ExecutionArgument{"deathAnimLength", "Single", 0, FreezeExecutionValue(deathAnimLength)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"wasRemovalPrevented", "Boolean", 0, FreezeExecutionValue(wasRemovalPrevented)}, ExecutionArgument{"deathAnimLength", "Single", 0, FreezeExecutionValue(deathAnimLength)}}; });
}

// Hooks/Hook.cs: AfterGoldGained
void AfterGoldGained(sts2::RunState runState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.AfterGoldGained", runState, player);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, CombatState())) {
        model.AfterGoldGained(player);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: AfterDiedToDoom
void AfterDiedToDoom(sts2::CombatState combatState, const sts2::CreatureList& creatures) {
    HookExecutionScope hook_execution_observation("Hook.AfterDiedToDoom", combatState, creatures);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateHookListenersOf(combatState)) {
        PlayerChoiceContext hookPlayerChoiceContext;
        model.AfterDiedToDoom(hookPlayerChoiceContext, creatures);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creatures", "IReadOnlyList<Creature>", 0, FreezeExecutionValue(creatures)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creatures", "IReadOnlyList<Creature>", 0, FreezeExecutionValue(creatures)}}; });
}

// Hooks/Hook.cs: AfterEnergyReset
void AfterEnergyReset(sts2::CombatState combatState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.AfterEnergyReset", combatState, player);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterEnergyReset(player);
    }
    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterEnergyResetLate(player);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: AfterEnergySpent
void AfterEnergySpent(sts2::CombatState combatState, sts2::CardModel card, int32_t amount) {
    HookExecutionScope hook_execution_observation("Hook.AfterEnergySpent", combatState, card, amount);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterEnergySpent(card, amount);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"amount", "Int32", 0, FreezeExecutionValue(amount)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"amount", "Int32", 0, FreezeExecutionValue(amount)}}; });
}

// Hooks/Hook.cs: BeforeFlush
void BeforeFlush(sts2::CombatState combatState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.BeforeFlush", combatState, player);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        PlayerChoiceContext playerChoiceContext;
        item.BeforeFlush(playerChoiceContext, player);
    }
    for (AbstractModel item2 : IterateCombatHookListeners(combatState)) {
        PlayerChoiceContext playerChoiceContext;
        item2.BeforeFlushLate(playerChoiceContext, player);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: AfterFlush
void AfterFlush(sts2::CombatState combatState, sts2::Player player, sts2::PlayerChoiceContext& playerChoiceContext, const sts2::CardList& flushedCards, const sts2::CardList& retainedCards) {
    HookExecutionScope hook_execution_observation("Hook.AfterFlush", combatState, player, playerChoiceContext, flushedCards, retainedCards);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        playerChoiceContext.PushModel(model);
        model.AfterFlush(playerChoiceContext, player, flushedCards, retainedCards);
        playerChoiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"playerChoiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(playerChoiceContext)}, ExecutionArgument{"flushedCards", "IReadOnlyCollection<CardModel>", 0, FreezeExecutionValue(flushedCards)}, ExecutionArgument{"retainedCards", "IReadOnlyCollection<CardModel>", 0, FreezeExecutionValue(retainedCards)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"playerChoiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(playerChoiceContext)}, ExecutionArgument{"flushedCards", "IReadOnlyCollection<CardModel>", 0, FreezeExecutionValue(flushedCards)}, ExecutionArgument{"retainedCards", "IReadOnlyCollection<CardModel>", 0, FreezeExecutionValue(retainedCards)}}; });
}

// Hooks/Hook.cs: AfterForge
void AfterForge(sts2::CombatState combatState, sts2::Dec amount, sts2::Player forger, sts2::AbstractModel source) {
    HookExecutionScope hook_execution_observation("Hook.AfterForge", combatState, amount, forger, source);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterForge(amount, forger, source);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"forger", "Player", 0, FreezeExecutionValue(forger)}, ExecutionArgument{"source", "AbstractModel", 0, FreezeExecutionValue(source)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"forger", "Player", 0, FreezeExecutionValue(forger)}, ExecutionArgument{"source", "AbstractModel", 0, FreezeExecutionValue(source)}}; });
}

// Hooks/Hook.cs: BeforeHandDraw
void BeforeHandDraw(sts2::CombatState combatState, sts2::Player player, sts2::PlayerChoiceContext& playerChoiceContext) {
    HookExecutionScope hook_execution_observation("Hook.BeforeHandDraw", combatState, player, playerChoiceContext);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        playerChoiceContext.PushModel(model);
        model.BeforeHandDraw(player, playerChoiceContext, combatState);
        playerChoiceContext.PopModel(model);
    }
    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        playerChoiceContext.PushModel(model);
        model.BeforeHandDrawLate(player, playerChoiceContext, combatState);
        playerChoiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"playerChoiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(playerChoiceContext)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"playerChoiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(playerChoiceContext)}}; });
}

// Hooks/Hook.cs: AfterHandEmptied
void AfterHandEmptied(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.AfterHandEmptied", combatState, choiceContext, player);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterHandEmptied(choiceContext, player);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: AfterModifyingBlockAmount
void AfterModifyingBlockAmount(sts2::CombatState combatState, sts2::Dec modifiedBlock, sts2::CardModel cardSource, const sts2::CardPlay* cardPlay, const sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingBlockAmount", combatState, modifiedBlock, cardSource, cardPlay, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateCombatHookListeners(combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingBlockAmount(modifiedBlock, cardSource, cardPlay);
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiedBlock", "Decimal", 0, FreezeExecutionValue(modifiedBlock)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiedBlock", "Decimal", 0, FreezeExecutionValue(modifiedBlock)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: AfterModifyingCardPlayCount
void AfterModifyingCardPlayCount(sts2::CombatState combatState, sts2::CardModel card, const sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingCardPlayCount", combatState, card, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateCombatHookListeners(combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingCardPlayCount(card);
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: AfterModifyingCardRewardOptions
void AfterModifyingCardRewardOptions(sts2::RunState runState, const sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingCardRewardOptions", runState, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateRunHookListeners(runState, CombatState())) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingCardRewardOptions();
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: AfterModifyingDamageAmount
void AfterModifyingDamageAmount(sts2::RunState runState, sts2::CombatState combatState, sts2::CardModel cardSource, const sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingDamageAmount", runState, combatState, cardSource, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateRunHookListeners(runState, combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingDamageAmount(cardSource);
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: AfterModifyingEnergyGain
void AfterModifyingEnergyGain(sts2::CombatState combatState, const sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingEnergyGain", combatState, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateCombatHookListeners(combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingEnergyGain();
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: AfterModifyingGoldGained
void AfterModifyingGoldGained(sts2::RunState runState, sts2::CombatState combatState, const sts2::ModelList& modifiers, sts2::Player player, sts2::Dec amount) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingGoldGained", runState, combatState, modifiers, player, amount);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateRunHookListeners(runState, combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingGoldGained(player, amount);
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; });
}

// Hooks/Hook.cs: AfterModifyingHandDraw
void AfterModifyingHandDraw(sts2::CombatState combatState, const sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingHandDraw", combatState, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateCombatHookListeners(combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingHandDraw();
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: AfterModifyingHpLostBeforeOsty
void AfterModifyingHpLostBeforeOsty(sts2::RunState runState, sts2::CombatState combatState, const sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingHpLostBeforeOsty", runState, combatState, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateRunHookListeners(runState, combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingHpLostBeforeOsty();
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: AfterModifyingHpLostAfterOsty
void AfterModifyingHpLostAfterOsty(sts2::RunState runState, sts2::CombatState combatState, const sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingHpLostAfterOsty", runState, combatState, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateRunHookListeners(runState, combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingHpLostAfterOsty();
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: AfterModifyingPowerAmountGiven
void AfterModifyingPowerAmountGiven(sts2::CombatState combatState, const sts2::ModelList& modifiers, sts2::PowerModel modifiedPower) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingPowerAmountGiven", combatState, modifiers, modifiedPower);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateCombatHookListeners(combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingPowerAmountGiven(modifiedPower);
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}, ExecutionArgument{"modifiedPower", "PowerModel", 0, FreezeExecutionValue(modifiedPower)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}, ExecutionArgument{"modifiedPower", "PowerModel", 0, FreezeExecutionValue(modifiedPower)}}; });
}

// Hooks/Hook.cs: AfterModifyingPowerAmountReceived
void AfterModifyingPowerAmountReceived(sts2::CombatState combatState, const sts2::ModelList& modifiers, sts2::PowerModel modifiedPower) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingPowerAmountReceived", combatState, modifiers, modifiedPower);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateCombatHookListeners(combatState)) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingPowerAmountReceived(modifiedPower);
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}, ExecutionArgument{"modifiedPower", "PowerModel", 0, FreezeExecutionValue(modifiedPower)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}, ExecutionArgument{"modifiedPower", "PowerModel", 0, FreezeExecutionValue(modifiedPower)}}; });
}

// Hooks/Hook.cs: AfterModifyingRewards
void AfterModifyingRewards(sts2::RunState runState, const sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingRewards", runState, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateRunHookListeners(runState, CombatState())) {
        if (list_contains(modifiers, modifier)) {
            modifier.AfterModifyingRewards();
        }
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: AfterOstyRevived
void AfterOstyRevived(sts2::CombatState combatState, sts2::Creature osty) {
    HookExecutionScope hook_execution_observation("Hook.AfterOstyRevived", combatState, osty);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterOstyRevived(osty);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"osty", "Creature", 0, FreezeExecutionValue(osty)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"osty", "Creature", 0, FreezeExecutionValue(osty)}}; });
}

// Hooks/Hook.cs: AfterPlayerTurnStart
void AfterPlayerTurnStart(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.AfterPlayerTurnStart", combatState, choiceContext, player);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterPlayerTurnStartEarly(choiceContext, player);
        choiceContext.PopModel(model);
    }
    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterPlayerTurnStart(choiceContext, player);
        choiceContext.PopModel(model);
    }
    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterPlayerTurnStartLate(choiceContext, player);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: AfterAutoPostPlayPhaseEntered
void AfterAutoPostPlayPhaseEntered(sts2::PlayerChoiceContext& playerChoiceContext, sts2::CombatState combatState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.AfterAutoPostPlayPhaseEntered", playerChoiceContext, combatState, player);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        playerChoiceContext.PushModel(model);
        model.AfterAutoPostPlayPhaseEntered(playerChoiceContext, player);
        playerChoiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"playerChoiceContext", "HookPlayerChoiceContext", 0, FreezeExecutionValue(playerChoiceContext)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"playerChoiceContext", "HookPlayerChoiceContext", 0, FreezeExecutionValue(playerChoiceContext)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: AfterAutoPrePlayPhaseEntered
void AfterAutoPrePlayPhaseEntered(sts2::PlayerChoiceContext& playerChoiceContext, sts2::CombatState combatState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.AfterAutoPrePlayPhaseEntered", playerChoiceContext, combatState, player);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        playerChoiceContext.PushModel(model);
        model.AfterAutoPrePlayPhaseEnteredEarly(playerChoiceContext, player);
        playerChoiceContext.PopModel(model);
    }
    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        playerChoiceContext.PushModel(model);
        model.AfterAutoPrePlayPhaseEntered(playerChoiceContext, player);
        playerChoiceContext.PopModel(model);
    }
    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        playerChoiceContext.PushModel(model);
        model.AfterAutoPrePlayPhaseEnteredLate(playerChoiceContext, player);
        playerChoiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"playerChoiceContext", "HookPlayerChoiceContext", 0, FreezeExecutionValue(playerChoiceContext)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"playerChoiceContext", "HookPlayerChoiceContext", 0, FreezeExecutionValue(playerChoiceContext)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: AfterPotionDiscarded
void AfterPotionDiscarded(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion) {
    HookExecutionScope hook_execution_observation("Hook.AfterPotionDiscarded", runState, combatState, potion);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.AfterPotionDiscarded(potion);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}}; });
}

// Hooks/Hook.cs: AfterPotionProcured
void AfterPotionProcured(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion) {
    HookExecutionScope hook_execution_observation("Hook.AfterPotionProcured", runState, combatState, potion);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.AfterPotionProcured(potion);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}}; });
}

// Hooks/Hook.cs: BeforePotionUsed
void BeforePotionUsed(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion, sts2::Creature target) {
    HookExecutionScope hook_execution_observation("Hook.BeforePotionUsed", runState, combatState, potion, target);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.BeforePotionUsed(potion, target);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}}; });
}

// Hooks/Hook.cs: AfterPotionUsed
void AfterPotionUsed(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion, sts2::Creature target) {
    HookExecutionScope hook_execution_observation("Hook.AfterPotionUsed", runState, combatState, potion, target);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, combatState)) {
        model.AfterPotionUsed(potion, target);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}}; });
}

// Hooks/Hook.cs: BeforePowerAmountChanged
void BeforePowerAmountChanged(sts2::CombatState combatState, sts2::PowerModel power, sts2::Dec amount, sts2::Creature target, sts2::Creature applier, sts2::CardModel cardSource) {
    HookExecutionScope hook_execution_observation("Hook.BeforePowerAmountChanged", combatState, power, amount, target, applier, cardSource);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel modifier : IterateCombatHookListeners(combatState)) {
        modifier.BeforePowerAmountChanged(power, amount, target, applier, cardSource);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"power", "PowerModel", 0, FreezeExecutionValue(power)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"applier", "Creature", 0, FreezeExecutionValue(applier)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"power", "PowerModel", 0, FreezeExecutionValue(power)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"applier", "Creature", 0, FreezeExecutionValue(applier)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; });
}

// Hooks/Hook.cs: AfterPowerAmountChanged
void AfterPowerAmountChanged(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::PowerModel power, sts2::Dec amount, sts2::Creature applier, sts2::CardModel cardSource) {
    HookExecutionScope hook_execution_observation("Hook.AfterPowerAmountChanged", combatState, choiceContext, power, amount, applier, cardSource);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterPowerAmountChanged(choiceContext, power, amount, applier, cardSource);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"power", "PowerModel", 0, FreezeExecutionValue(power)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"applier", "Creature", 0, FreezeExecutionValue(applier)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"power", "PowerModel", 0, FreezeExecutionValue(power)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"applier", "Creature", 0, FreezeExecutionValue(applier)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}}; });
}

// Hooks/Hook.cs: AfterPreventingBlockClear
void AfterPreventingBlockClear(sts2::CombatState combatState, sts2::AbstractModel preventer, sts2::Creature creature) {
    HookExecutionScope hook_execution_observation("Hook.AfterPreventingBlockClear", combatState, preventer, creature);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    if (range_contains(IterateCombatHookListeners(combatState), preventer)) {
        preventer.AfterPreventingBlockClear(preventer, creature);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"preventer", "AbstractModel", 0, FreezeExecutionValue(preventer)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"preventer", "AbstractModel", 0, FreezeExecutionValue(preventer)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; });
}

// Hooks/Hook.cs: AfterPreventingDeath
void AfterPreventingDeath(sts2::RunState runState, sts2::CombatState combatState, sts2::AbstractModel preventer, sts2::Creature creature) {
    HookExecutionScope hook_execution_observation("Hook.AfterPreventingDeath", runState, combatState, preventer, creature);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    if (range_contains(IterateRunHookListeners(runState, combatState), preventer)) {
        preventer.AfterPreventingDeath(creature);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"preventer", "AbstractModel", 0, FreezeExecutionValue(preventer)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"preventer", "AbstractModel", 0, FreezeExecutionValue(preventer)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; });
}

// Hooks/Hook.cs: AfterPreventingDraw
void AfterPreventingDraw(sts2::CombatState combatState, sts2::AbstractModel modifier) {
    HookExecutionScope hook_execution_observation("Hook.AfterPreventingDraw", combatState, modifier);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    if (range_contains(IterateCombatHookListeners(combatState), modifier)) {
        modifier.AfterPreventingDraw();
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifier", "AbstractModel", 0, FreezeExecutionValue(modifier)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"modifier", "AbstractModel", 0, FreezeExecutionValue(modifier)}}; });
}

// Hooks/Hook.cs: AfterRestSiteHeal
void AfterRestSiteHeal(sts2::RunState runState, sts2::Player player, bool isMimicked) {
    HookExecutionScope hook_execution_observation("Hook.AfterRestSiteHeal", runState, player, isMimicked);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, CombatState())) {
        model.AfterRestSiteHeal(player, isMimicked);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"isMimicked", "Boolean", 0, FreezeExecutionValue(isMimicked)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"isMimicked", "Boolean", 0, FreezeExecutionValue(isMimicked)}}; });
}

// Hooks/Hook.cs: AfterRestSiteSmith
void AfterRestSiteSmith(sts2::RunState runState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.AfterRestSiteSmith", runState, player);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, CombatState())) {
        model.AfterRestSiteSmith(player);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: BeforeRoomEntered
void BeforeRoomEntered(sts2::RunState runState, sts2::AbstractRoom room) {
    HookExecutionScope hook_execution_observation("Hook.BeforeRoomEntered", runState, room);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, CombatState())) {
        model.BeforeRoomEntered(room);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"room", "AbstractRoom", 0, FreezeExecutionValue(room)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"room", "AbstractRoom", 0, FreezeExecutionValue(room)}}; });
}

// Hooks/Hook.cs: AfterRoomEntered
void AfterRoomEntered(sts2::RunState runState, sts2::AbstractRoom room) {
    HookExecutionScope hook_execution_observation("Hook.AfterRoomEntered", runState, room);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, CombatState())) {
        model.AfterRoomEntered(room);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"room", "AbstractRoom", 0, FreezeExecutionValue(room)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"room", "AbstractRoom", 0, FreezeExecutionValue(room)}}; });
}

// Hooks/Hook.cs: AfterShuffle
void AfterShuffle(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Player shuffler) {
    HookExecutionScope hook_execution_observation("Hook.AfterShuffle", combatState, choiceContext, shuffler);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterShuffle(choiceContext, shuffler);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"shuffler", "Player", 0, FreezeExecutionValue(shuffler)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"shuffler", "Player", 0, FreezeExecutionValue(shuffler)}}; });
}

// Hooks/Hook.cs: BeforeSideTurnStart
void BeforeSideTurnStart(sts2::CombatState combatState, sts2::CombatSide side, const sts2::CreatureList& participants) {
    HookExecutionScope hook_execution_observation("Hook.BeforeSideTurnStart", combatState, side, participants);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        PlayerChoiceContext hookPlayerChoiceContext;
        model.BeforeSideTurnStart(hookPlayerChoiceContext, side, participants, combatState);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"side", "CombatSide", 0, FreezeExecutionValue(side)}, ExecutionArgument{"participants", "IReadOnlyList<Creature>", 0, FreezeExecutionValue(participants)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"side", "CombatSide", 0, FreezeExecutionValue(side)}, ExecutionArgument{"participants", "IReadOnlyList<Creature>", 0, FreezeExecutionValue(participants)}}; });
}

// Hooks/Hook.cs: AfterSideTurnStart
void AfterSideTurnStart(sts2::CombatState combatState, sts2::CombatSide side, const sts2::CreatureList& participants) {
    HookExecutionScope hook_execution_observation("Hook.AfterSideTurnStart", combatState, side, participants);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterSideTurnStart(side, participants, combatState);
    }
    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterSideTurnStartLate(side, participants, combatState);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"side", "CombatSide", 0, FreezeExecutionValue(side)}, ExecutionArgument{"participants", "IReadOnlyList<Creature>", 0, FreezeExecutionValue(participants)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"side", "CombatSide", 0, FreezeExecutionValue(side)}, ExecutionArgument{"participants", "IReadOnlyList<Creature>", 0, FreezeExecutionValue(participants)}}; });
}

// Hooks/Hook.cs: AfterStarsGained
void AfterStarsGained(sts2::CombatState combatState, int32_t amount, sts2::Player gainer) {
    HookExecutionScope hook_execution_observation("Hook.AfterStarsGained", combatState, amount, gainer);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterStarsGained(amount, gainer);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Int32", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"gainer", "Player", 0, FreezeExecutionValue(gainer)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Int32", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"gainer", "Player", 0, FreezeExecutionValue(gainer)}}; });
}

// Hooks/Hook.cs: AfterStarsSpent
void AfterStarsSpent(sts2::CombatState combatState, int32_t amount, sts2::Player spender) {
    HookExecutionScope hook_execution_observation("Hook.AfterStarsSpent", combatState, amount, spender);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterStarsSpent(amount, spender);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Int32", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"spender", "Player", 0, FreezeExecutionValue(spender)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Int32", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"spender", "Player", 0, FreezeExecutionValue(spender)}}; });
}

// Hooks/Hook.cs: AfterSummon
void AfterSummon(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Player summoner, sts2::Dec amount) {
    HookExecutionScope hook_execution_observation("Hook.AfterSummon", combatState, choiceContext, summoner, amount);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(model);
        model.AfterSummon(choiceContext, summoner, amount);
        choiceContext.PopModel(model);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"summoner", "Player", 0, FreezeExecutionValue(summoner)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"summoner", "Player", 0, FreezeExecutionValue(summoner)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; });
}

// Hooks/Hook.cs: AfterTakingExtraTurn
void AfterTakingExtraTurn(sts2::CombatState combatState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.AfterTakingExtraTurn", combatState, player);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateCombatHookListeners(combatState)) {
        model.AfterTakingExtraTurn(player);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: BeforeSideTurnEnd
void BeforeSideTurnEnd(sts2::CombatState combatState, sts2::CombatSide side, const sts2::CreatureList& participants) {
    HookExecutionScope hook_execution_observation("Hook.BeforeSideTurnEnd", combatState, side, participants);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        PlayerChoiceContext playerChoiceContext;
        item.BeforeSideTurnEndVeryEarly(playerChoiceContext, side, participants);
    }
    for (AbstractModel item2 : IterateCombatHookListeners(combatState)) {
        PlayerChoiceContext playerChoiceContext;
        item2.BeforeSideTurnEndEarly(playerChoiceContext, side, participants);
    }
    for (AbstractModel item3 : IterateCombatHookListeners(combatState)) {
        PlayerChoiceContext playerChoiceContext;
        item3.BeforeSideTurnEnd(playerChoiceContext, side, participants);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"side", "CombatSide", 0, FreezeExecutionValue(side)}, ExecutionArgument{"participants", "IEnumerable<Creature>", 0, FreezeExecutionValue(participants)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"side", "CombatSide", 0, FreezeExecutionValue(side)}, ExecutionArgument{"participants", "IEnumerable<Creature>", 0, FreezeExecutionValue(participants)}}; });
}

// Hooks/Hook.cs: AfterSideTurnEnd
void AfterSideTurnEnd(sts2::CombatState combatState, sts2::CombatSide side, const sts2::CreatureList& participants) {
    HookExecutionScope hook_execution_observation("Hook.AfterSideTurnEnd", combatState, side, participants);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        PlayerChoiceContext playerChoiceContext;
        item.AfterSideTurnEnd(playerChoiceContext, side, participants);
    }
    for (AbstractModel item2 : IterateCombatHookListeners(combatState)) {
        PlayerChoiceContext playerChoiceContext;
        item2.AfterSideTurnEndLate(playerChoiceContext, side, participants);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"side", "CombatSide", 0, FreezeExecutionValue(side)}, ExecutionArgument{"participants", "IEnumerable<Creature>", 0, FreezeExecutionValue(participants)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"side", "CombatSide", 0, FreezeExecutionValue(side)}, ExecutionArgument{"participants", "IEnumerable<Creature>", 0, FreezeExecutionValue(participants)}}; });
}

// Hooks/Hook.cs: ModifyAttackHitCount
sts2::Dec ModifyAttackHitCount(sts2::CombatState combatState, sts2::AttackCommand& attackCommand, int32_t originalHitCount) {
    HookExecutionScope hook_execution_observation("Hook.ModifyAttackHitCount", combatState, attackCommand, originalHitCount);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    int32_t num = originalHitCount;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        num = item.ModifyAttackHitCount(attackCommand, num);
    }
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"attackCommand", "AttackCommand", 0, FreezeExecutionValue(attackCommand)}, ExecutionArgument{"originalHitCount", "Int32", 0, FreezeExecutionValue(originalHitCount)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"attackCommand", "AttackCommand", 0, FreezeExecutionValue(attackCommand)}, ExecutionArgument{"originalHitCount", "Int32", 0, FreezeExecutionValue(originalHitCount)}}; });
}

// Hooks/Hook.cs: ModifyBlock
sts2::Dec ModifyBlock(sts2::CombatState combatState, sts2::Creature target, sts2::Dec block, sts2::ValueProp props, sts2::CardModel cardSource, const sts2::CardPlay* cardPlay, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyBlock", combatState, target, block, props, cardSource, cardPlay, modifiers);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    ModelList list;
    Dec num = block;
    if (cardSource && cardSource.Enchantment()) {
        EnchantmentModel enchantment = cardSource.Enchantment();
        num = num + enchantment.EnchantBlockAdditive(num);
        num = num * enchantment.EnchantBlockMultiplicative(num);
    }
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        Dec num2 = item.ModifyBlockAdditive(target, num, props, cardSource, cardPlay);
        num = num + num2;
        if (num2 != Dec(0)) list.push_back(item);
    }
    for (AbstractModel item2 : IterateCombatHookListeners(combatState)) {
        Dec num3 = item2.ModifyBlockMultiplicative(target, num, props, cardSource, cardPlay);
        num = num * num3;
        if (num3 != Dec(1)) list.push_back(item2);
    }
    modifiers = list;
    return dec_max(Dec(0), num);
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"block", "Decimal", 0, FreezeExecutionValue(block)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"block", "Decimal", 0, FreezeExecutionValue(block)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyCardBeingAddedToDeck
sts2::CardModel ModifyCardBeingAddedToDeck(sts2::RunState runState, sts2::CardModel card, sts2::ModelList& modifyingModels) {
    HookExecutionScope hook_execution_observation("Hook.ModifyCardBeingAddedToDeck", runState, card, modifyingModels);
    return hook_execution_observation.Invoke("CardModel", [&]() -> sts2::CardModel {
    // STS2_EXECUTION_BODY_BEGIN

    modifyingModels.clear();
    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        CardModel newCard;
        if (item.TryModifyCardBeingAddedToDeck(card, newCard) && newCard) {
            modifyingModels.push_back(item);
            card = newCard;
        }
    }
    for (AbstractModel item2 : IterateRunHookListeners(runState, CombatState())) {
        CardModel newCard2;
        if (item2.TryModifyCardBeingAddedToDeckLate(card, newCard2) && newCard2) {
            modifyingModels.push_back(item2);
            card = newCard2;
        }
    }
    return card;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"modifyingModels", "List<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"modifyingModels", "List<AbstractModel>&", 3, FreezeExecutionValue(modifyingModels)}}; });
}

// Hooks/Hook.cs: ModifyCardPlayCount
int32_t ModifyCardPlayCount(sts2::CombatState combatState, sts2::CardModel card, int32_t playCount, sts2::Creature target, sts2::ModelList& modifyingModels) {
    HookExecutionScope hook_execution_observation("Hook.ModifyCardPlayCount", combatState, card, playCount, target, modifyingModels);
    return hook_execution_observation.Invoke("Int32", [&]() -> int32_t {
    // STS2_EXECUTION_BODY_BEGIN

    modifyingModels.clear();
    int32_t num = playCount;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        int32_t num2 = num;
        num = item.ModifyCardPlayCount(card, target, num);
        if (num != num2) {
            modifyingModels.push_back(item);
        }
    }
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"playCount", "Int32", 0, FreezeExecutionValue(playCount)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"modifyingModels", "List<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"playCount", "Int32", 0, FreezeExecutionValue(playCount)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"modifyingModels", "List<AbstractModel>&", 3, FreezeExecutionValue(modifyingModels)}}; });
}

// Hooks/Hook.cs: ModifyCardPlayResultLocation
sts2::CardLocation ModifyCardPlayResultLocation(sts2::CombatState combatState, sts2::CardModel card, bool isAutoPlay, const sts2::ResourceInfo& resources, sts2::CardLocation cardLocation, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyCardPlayResultLocation", combatState, card, isAutoPlay, resources, cardLocation, modifiers);
    return hook_execution_observation.Invoke("CardLocation", [&]() -> sts2::CardLocation {
    // STS2_EXECUTION_BODY_BEGIN

    ModelList list;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        CardLocation cardLocation2 = cardLocation;
        cardLocation = item.ModifyCardPlayResultLocation(card, isAutoPlay, resources, cardLocation);
        if (!(cardLocation2 == cardLocation)) list.push_back(item);
    }
    modifiers = list;
    return cardLocation;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"isAutoPlay", "Boolean", 0, FreezeExecutionValue(isAutoPlay)}, ExecutionArgument{"resources", "ResourceInfo", 0, FreezeExecutionValue(resources)}, ExecutionArgument{"cardLocation", "CardLocation", 0, FreezeExecutionValue(cardLocation)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"isAutoPlay", "Boolean", 0, FreezeExecutionValue(isAutoPlay)}, ExecutionArgument{"resources", "ResourceInfo", 0, FreezeExecutionValue(resources)}, ExecutionArgument{"cardLocation", "CardLocation", 0, FreezeExecutionValue(cardLocation)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyCardRewardUpgradeOdds
sts2::Dec ModifyCardRewardUpgradeOdds(sts2::RunState runState, sts2::Player player, sts2::CardModel card, sts2::Dec originalOdds) {
    HookExecutionScope hook_execution_observation("Hook.ModifyCardRewardUpgradeOdds", runState, player, card, originalOdds);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = originalOdds;
    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        num = item.ModifyCardRewardUpgradeOdds(player, card, num);
    }
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"originalOdds", "Decimal", 0, FreezeExecutionValue(originalOdds)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"originalOdds", "Decimal", 0, FreezeExecutionValue(originalOdds)}}; });
}

// Hooks/Hook.cs: ModifyDamage
sts2::Dec ModifyDamage(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature target, sts2::Creature dealer, sts2::Dec damage, sts2::ValueProp props, sts2::CardModel cardSource, const sts2::CardPlay* cardPlay, sts2::ModifyDamageHookType modifyDamageHookType, sts2::CardPreviewMode previewMode, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyDamage", runState, combatState, target, dealer, damage, props, cardSource, cardPlay, modifyDamageHookType, previewMode, modifiers);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    ModelList modifiers2;
    Dec num = damage;
    if (cardSource && cardSource.Enchantment()) {
        if (has_flag(modifyDamageHookType, ModifyDamageHookType::Additive)) {
            num = num + cardSource.Enchantment().EnchantDamageAdditive(num, props);
        }
        if (has_flag(modifyDamageHookType, ModifyDamageHookType::Multiplicative)) {
            num = num * cardSource.Enchantment().EnchantDamageMultiplicative(num, props);
        }
    }
    // Preview of a multi-target card with no target: the damage every hittable
    // enemy would take, if they all agree.
    bool flag4 = false;
    if (!target && previewMode == CardPreviewMode::MultiCreatureTargeting && cardSource) {
        TargetType targetType = cardSource.TargetType();
        if (static_cast<uint32_t>(static_cast<int>(targetType) - 3) <= 1u) {
            CardPile pile = cardSource.Pile();
            flag4 = pile && (pile.Type() == PileType::Hand || pile.Type() == PileType::Play);
        }
    }
    bool flag5 = false;
    if (flag4) {
        bool flag6 = true;
        bool hasNum2 = false;
        Dec num2;
        CreatureList enemies = combatState ? combatState.HittableEnemies() : CreatureList{};
        for (Creature item : enemies) {
            ModelList modifiers3;
            Dec num3 = ModifyDamageInternal(runState, combatState, item, dealer, num, props, cardSource, cardPlay,
                                            modifyDamageHookType, modifiers3);
            if (!hasNum2) {
                num2 = num3;
                hasNum2 = true;
            } else if (num3.to_int() != num2.to_int()) {
                flag6 = false;
                break;
            }
            for (AbstractModel m : modifiers3) modifiers2.push_back(m);
        }
        if (hasNum2 && flag6) {
            flag5 = true;
            num = num2;
            ModelList distinct;  // Distinct(): first occurrences, in order
            for (AbstractModel m : modifiers2) {
                if (!list_contains(distinct, m)) distinct.push_back(m);
            }
            modifiers2 = distinct;
        } else {
            modifiers2.clear();
        }
    }
    if (!flag4 || !flag5) {
        num = ModifyDamageInternal(runState, combatState, target, dealer, num, props, cardSource, cardPlay,
                                   modifyDamageHookType, modifiers2);
    }
    modifiers = modifiers2;
    return dec_max(Dec(0), num);
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"damage", "Decimal", 0, FreezeExecutionValue(damage)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}, ExecutionArgument{"modifyDamageHookType", "ModifyDamageHookType", 0, FreezeExecutionValue(modifyDamageHookType)}, ExecutionArgument{"previewMode", "CardPreviewMode", 0, FreezeExecutionValue(previewMode)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"damage", "Decimal", 0, FreezeExecutionValue(damage)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"cardPlay", "CardPlay", 0, FreezeExecutionValue(cardPlay)}, ExecutionArgument{"modifyDamageHookType", "ModifyDamageHookType", 0, FreezeExecutionValue(modifyDamageHookType)}, ExecutionArgument{"previewMode", "CardPreviewMode", 0, FreezeExecutionValue(previewMode)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyEnergyCostInCombat
sts2::Dec ModifyEnergyCostInCombat(sts2::CombatState combatState, sts2::CardModel card, sts2::Dec originalCost) {
    HookExecutionScope hook_execution_observation("Hook.ModifyEnergyCostInCombat", combatState, card, originalCost);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    if (originalCost < Dec(0)) return originalCost;
    Dec modifiedCost = originalCost;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        item.TryModifyEnergyCostInCombat(card, modifiedCost, modifiedCost);
    }
    for (AbstractModel item2 : IterateCombatHookListeners(combatState)) {
        item2.TryModifyEnergyCostInCombatLate(card, modifiedCost, modifiedCost);
    }
    return modifiedCost;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"originalCost", "Decimal", 0, FreezeExecutionValue(originalCost)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"originalCost", "Decimal", 0, FreezeExecutionValue(originalCost)}}; });
}

// Hooks/Hook.cs: ModifyKeywordsInCombat
void ModifyKeywordsInCombat(sts2::CombatState combatState, sts2::CardModel card, sts2::CardKeywordSet& keywords) {
    HookExecutionScope hook_execution_observation("Hook.ModifyKeywordsInCombat", combatState, card, keywords);
    return hook_execution_observation.Invoke("Void", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateHookListenersOf(combatState)) {
        item.TryModifyKeywordsInCombat(card, keywords);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"keywords", "ISet<CardKeyword>", 0, FreezeExecutionValue(keywords)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"keywords", "ISet<CardKeyword>", 0, FreezeExecutionValue(keywords)}}; });
}

// Hooks/Hook.cs: ModifyEnergyGain
sts2::Dec ModifyEnergyGain(sts2::CombatState combatState, sts2::Player player, sts2::Dec originalAmount, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyEnergyGain", combatState, player, originalAmount, modifiers);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = originalAmount;
    ModelList list;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        Dec num2 = num;
        num = item.ModifyEnergyGain(player, num);
        if (num2.to_int() != num.to_int()) {
            list.push_back(item);
        }
    }
    modifiers = list;
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"originalAmount", "Decimal", 0, FreezeExecutionValue(originalAmount)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"originalAmount", "Decimal", 0, FreezeExecutionValue(originalAmount)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyGoldGained
sts2::Dec ModifyGoldGained(sts2::RunState runState, sts2::CombatState combatState, sts2::Dec amount, sts2::Player player, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyGoldGained", runState, combatState, amount, player, modifiers);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = amount;
    ModelList list;
    for (AbstractModel item : IterateRunHookListeners(runState, combatState)) {
        Dec num2 = num;
        num = item.ModifyGoldGained(player, num);
        if (num2.to_int() != num.to_int()) {
            list.push_back(item);
        }
    }
    modifiers = list;
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyHandDraw
sts2::Dec ModifyHandDraw(sts2::CombatState combatState, sts2::Player player, sts2::Dec originalCardCount, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyHandDraw", combatState, player, originalCardCount, modifiers);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = originalCardCount;
    ModelList list;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        Dec num2 = num;
        num = item.ModifyHandDraw(player, num);
        if (num2.to_int() != num.to_int()) {
            list.push_back(item);
        }
    }
    for (AbstractModel item2 : IterateCombatHookListeners(combatState)) {
        Dec num3 = num;
        num = item2.ModifyHandDrawLate(player, num);
        if (num3.to_int() != num.to_int()) {
            list.push_back(item2);
        }
    }
    modifiers = list;
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"originalCardCount", "Decimal", 0, FreezeExecutionValue(originalCardCount)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"originalCardCount", "Decimal", 0, FreezeExecutionValue(originalCardCount)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyHpLost
sts2::Dec ModifyHpLost(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature target, sts2::Dec amount, sts2::ValueProp props, sts2::Creature dealer, sts2::CardModel cardSource, sts2::HpLossHookPhase phases, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyHpLost", runState, combatState, target, amount, props, dealer, cardSource, phases, modifiers);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = amount;
    ModelList list;
    if (has_flag(phases, HpLossHookPhase::BeforeOsty)) {
        for (AbstractModel item : IterateRunHookListeners(runState, combatState)) {
            Dec d = num;
            num = item.ModifyHpLostBeforeOsty(target, num, props, dealer, cardSource);
            if (dec_truncate(d) != dec_truncate(num)) {
                list.push_back(item);
            }
        }
        for (AbstractModel item2 : IterateRunHookListeners(runState, combatState)) {
            Dec d2 = num;
            num = item2.ModifyHpLostBeforeOstyLate(target, num, props, dealer, cardSource);
            if (dec_truncate(d2) != dec_truncate(num)) {
                list.push_back(item2);
            }
        }
    }
    if (has_flag(phases, HpLossHookPhase::AfterOsty)) {
        for (AbstractModel item3 : IterateRunHookListeners(runState, combatState)) {
            Dec d3 = num;
            num = item3.ModifyHpLostAfterOsty(target, num, props, dealer, cardSource);
            if (dec_truncate(d3) != dec_truncate(num)) {
                list.push_back(item3);
            }
        }
        for (AbstractModel item4 : IterateRunHookListeners(runState, combatState)) {
            Dec d4 = num;
            num = item4.ModifyHpLostAfterOstyLate(target, num, props, dealer, cardSource);
            if (dec_truncate(d4) != dec_truncate(num)) {
                list.push_back(item4);
            }
        }
    }
    modifiers = list;
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"phases", "HpLossHookPhase", 0, FreezeExecutionValue(phases)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"phases", "HpLossHookPhase", 0, FreezeExecutionValue(phases)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyMaxEnergy
sts2::Dec ModifyMaxEnergy(sts2::CombatState combatState, sts2::Player player, sts2::Dec amount) {
    HookExecutionScope hook_execution_observation("Hook.ModifyMaxEnergy", combatState, player, amount);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = amount;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        num = item.ModifyMaxEnergy(player, num);
    }
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; });
}

// Hooks/Hook.cs: ModifyMerchantCardRarity
sts2::CardRarity ModifyMerchantCardRarity(sts2::RunState runState, sts2::Player player, sts2::CardRarity rarity) {
    HookExecutionScope hook_execution_observation("Hook.ModifyMerchantCardRarity", runState, player, rarity);
    return hook_execution_observation.Invoke("CardRarity", [&]() -> sts2::CardRarity {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        rarity = item.ModifyMerchantCardRarity(player, rarity);
    }
    return rarity;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"rarity", "CardRarity", 0, FreezeExecutionValue(rarity)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"rarity", "CardRarity", 0, FreezeExecutionValue(rarity)}}; });
}

// Hooks/Hook.cs: ModifyMerchantPrice
sts2::Dec ModifyMerchantPrice(sts2::RunState runState, sts2::Player player, sts2::Merchant::EntryRef entry,
                              sts2::Dec price) {
    HookExecutionScope hook_execution_observation("Hook.ModifyMerchantPrice", runState, player, entry, price);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        price = item.ModifyMerchantPrice(player, entry, price);
    }
    return price;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"entry", "MerchantEntry", 0, FreezeExecutionValue(entry)}, ExecutionArgument{"result", "Decimal", 0, FreezeExecutionValue(price)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"entry", "MerchantEntry", 0, FreezeExecutionValue(entry)}, ExecutionArgument{"result", "Decimal", 0, FreezeExecutionValue(price)}}; });
}

// Hooks/Hook.cs: ModifyOddsIncreaseForUnrolledRoomType
float ModifyOddsIncreaseForUnrolledRoomType(sts2::RunState runState, sts2::RoomType roomType, float oddsIncrease) {
    HookExecutionScope hook_execution_observation("Hook.ModifyOddsIncreaseForUnrolledRoomType", runState, roomType, oddsIncrease);
    return hook_execution_observation.Invoke("Single", [&]() -> float {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        oddsIncrease = item.ModifyOddsIncreaseForUnrolledRoomType(roomType, oddsIncrease);
    }
    return oddsIncrease;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"roomType", "RoomType", 0, FreezeExecutionValue(roomType)}, ExecutionArgument{"oddsIncrease", "Single", 0, FreezeExecutionValue(oddsIncrease)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"roomType", "RoomType", 0, FreezeExecutionValue(roomType)}, ExecutionArgument{"oddsIncrease", "Single", 0, FreezeExecutionValue(oddsIncrease)}}; });
}

// Hooks/Hook.cs: ModifyNextEvent.
sts2::EventModel ModifyNextEvent(sts2::RunState runState, sts2::EventModel currentEvent) {
    HookExecutionScope hook_execution_observation("Hook.ModifyNextEvent", runState, currentEvent);
    return hook_execution_observation.Invoke("EventModel", [&]() -> sts2::EventModel {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        currentEvent = item.ModifyNextEvent(currentEvent);
    }
    return currentEvent;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"currentEvent", "EventModel", 0, FreezeExecutionValue(currentEvent)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"currentEvent", "EventModel", 0, FreezeExecutionValue(currentEvent)}}; });
}

// Hooks/Hook.cs: ModifyGeneratedMap followed by ModifyGeneratedMapLate.
sts2::ActMap ModifyGeneratedMap(sts2::RunState runState, sts2::ActMap map, int32_t actIndex) {
    HookExecutionScope hook_execution_observation("Hook.ModifyGeneratedMap", runState, map, actIndex);
    return hook_execution_observation.Invoke("ActMap", [&]() -> sts2::ActMap {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        map = item.ModifyGeneratedMap(runState, map, actIndex);
    }
    return ModifyGeneratedMapLate(runState, map, actIndex);
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"map", "ActMap", 0, FreezeExecutionValue(map)}, ExecutionArgument{"actIndex", "Int32", 0, FreezeExecutionValue(actIndex)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"map", "ActMap", 0, FreezeExecutionValue(map)}, ExecutionArgument{"actIndex", "Int32", 0, FreezeExecutionValue(actIndex)}}; });
}

sts2::ActMap ModifyGeneratedMapLate(sts2::RunState runState, sts2::ActMap map, int32_t actIndex) {
    HookExecutionScope hook_execution_observation("Hook.ModifyGeneratedMapLate", runState, map, actIndex);
    return hook_execution_observation.Invoke("ActMap", [&]() -> sts2::ActMap {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        map = item.ModifyGeneratedMapLate(runState, map, actIndex);
    }
    return map;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"map", "ActMap", 0, FreezeExecutionValue(map)}, ExecutionArgument{"actIndex", "Int32", 0, FreezeExecutionValue(actIndex)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"map", "ActMap", 0, FreezeExecutionValue(map)}, ExecutionArgument{"actIndex", "Int32", 0, FreezeExecutionValue(actIndex)}}; });
}

// Hooks/Hook.cs: AfterMapGenerated. ActMap is reference-typed in the game,
// so this callback mutates the generated map in place.
void AfterMapGenerated(sts2::RunState runState, sts2::ActMap& map, int32_t actIndex) {
    HookExecutionScope hook_execution_observation("Hook.AfterMapGenerated", runState, map, actIndex);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        item.AfterMapGenerated(map, actIndex);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"map", "ActMap", 0, FreezeExecutionValue(map)}, ExecutionArgument{"actIndex", "Int32", 0, FreezeExecutionValue(actIndex)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"map", "ActMap", 0, FreezeExecutionValue(map)}, ExecutionArgument{"actIndex", "Int32", 0, FreezeExecutionValue(actIndex)}}; });
}

// Hooks/Hook.cs: ModifyUnknownMapPointRoomTypes. RoomTypeSet is the POD
// bit-set representation of IReadOnlySet<RoomType>.
sts2::RoomTypeSet ModifyUnknownMapPointRoomTypes(sts2::RunState runState, sts2::RoomTypeSet roomTypes) {
    HookExecutionScope hook_execution_observation("Hook.ModifyUnknownMapPointRoomTypes", runState, roomTypes);
    return hook_execution_observation.Invoke("IReadOnlySet<RoomType>", [&]() -> sts2::RoomTypeSet {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        roomTypes = item.ModifyUnknownMapPointRoomTypes(roomTypes);
    }
    return roomTypes;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"roomTypes", "IReadOnlySet<RoomType>", 0, FreezeExecutionValue(roomTypes)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"roomTypes", "IReadOnlySet<RoomType>", 0, FreezeExecutionValue(roomTypes)}}; });
}

// Hooks/Hook.cs: ModifyPowerAmountGiven
sts2::Dec ModifyPowerAmountGiven(sts2::CombatState combatState, sts2::PowerModel power, sts2::Creature giver, sts2::Dec amount, sts2::Creature target, sts2::CardModel cardSource, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyPowerAmountGiven", combatState, power, giver, amount, target, cardSource, modifiers);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = amount;
    ModelList list;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        Dec num2 = item.ModifyPowerAmountGivenAdditive(power, giver, num, target, cardSource);
        num += num2;
        if (num2 != Dec(0)) {
            list.push_back(item);
        }
    }
    for (AbstractModel item2 : IterateCombatHookListeners(combatState)) {
        Dec num3 = item2.ModifyPowerAmountGivenMultiplicative(power, giver, num, target, cardSource);
        num *= num3;
        if (num3 != Dec(1)) {
            list.push_back(item2);
        }
    }
    modifiers = list;
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"power", "PowerModel", 0, FreezeExecutionValue(power)}, ExecutionArgument{"giver", "Creature", 0, FreezeExecutionValue(giver)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"power", "PowerModel", 0, FreezeExecutionValue(power)}, ExecutionArgument{"giver", "Creature", 0, FreezeExecutionValue(giver)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"cardSource", "CardModel", 0, FreezeExecutionValue(cardSource)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyPowerAmountReceived
sts2::Dec ModifyPowerAmountReceived(sts2::CombatState combatState, sts2::PowerModel canonicalPower, sts2::Creature target, sts2::Dec amount, sts2::Creature giver, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyPowerAmountReceived", combatState, canonicalPower, target, amount, giver, modifiers);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = amount;
    ModelList list;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        Dec modifiedAmount;
        if (item.TryModifyPowerAmountReceived(canonicalPower, target, num, giver, modifiedAmount)) {
            num = modifiedAmount;
            list.push_back(item);
        }
    }
    modifiers = list;
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"canonicalPower", "PowerModel", 0, FreezeExecutionValue(canonicalPower)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"giver", "Creature", 0, FreezeExecutionValue(giver)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"canonicalPower", "PowerModel", 0, FreezeExecutionValue(canonicalPower)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"giver", "Creature", 0, FreezeExecutionValue(giver)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyRestSiteHealAmount
sts2::Dec ModifyRestSiteHealAmount(sts2::RunState runState, sts2::Creature creature, sts2::Dec amount) {
    HookExecutionScope hook_execution_observation("Hook.ModifyRestSiteHealAmount", runState, creature, amount);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = amount;
    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        num = item.ModifyRestSiteHealAmount(creature, num);
    }
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; });
}

// Hooks/Hook.cs: ModifyShuffleOrder
void ModifyShuffleOrder(sts2::CombatState combatState, sts2::Player player, sts2::CardList& cards, bool isInitialShuffle) {
    HookExecutionScope hook_execution_observation("Hook.ModifyShuffleOrder", combatState, player, cards, isInitialShuffle);
    return hook_execution_observation.Invoke("Void", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        item.ModifyShuffleOrder(player, cards, isInitialShuffle);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"cards", "List<CardModel>", 0, FreezeExecutionValue(cards)}, ExecutionArgument{"isInitialShuffle", "Boolean", 0, FreezeExecutionValue(isInitialShuffle)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"cards", "List<CardModel>", 0, FreezeExecutionValue(cards)}, ExecutionArgument{"isInitialShuffle", "Boolean", 0, FreezeExecutionValue(isInitialShuffle)}}; });
}

// Hooks/Hook.cs: ModifyStarCost
sts2::Dec ModifyStarCost(sts2::CombatState combatState, sts2::CardModel card, sts2::Dec originalCost) {
    HookExecutionScope hook_execution_observation("Hook.ModifyStarCost", combatState, card, originalCost);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    if (originalCost < Dec(0)) return originalCost;
    Dec modifiedCost = originalCost;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        item.TryModifyStarCost(card, modifiedCost, modifiedCost);
    }
    return modifiedCost;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"originalCost", "Decimal", 0, FreezeExecutionValue(originalCost)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"originalCost", "Decimal", 0, FreezeExecutionValue(originalCost)}}; });
}

// Hooks/Hook.cs: ModifySummonAmount
sts2::Dec ModifySummonAmount(sts2::CombatState combatState, sts2::Player summoner, sts2::Dec amount, sts2::AbstractModel source) {
    HookExecutionScope hook_execution_observation("Hook.ModifySummonAmount", combatState, summoner, amount, source);
    return hook_execution_observation.Invoke("Decimal", [&]() -> sts2::Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec num = amount;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        num = item.ModifySummonAmount(summoner, num, source);
    }
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"summoner", "Player", 0, FreezeExecutionValue(summoner)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"source", "AbstractModel", 0, FreezeExecutionValue(source)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"summoner", "Player", 0, FreezeExecutionValue(summoner)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"source", "AbstractModel", 0, FreezeExecutionValue(source)}}; });
}

// Hooks/Hook.cs: ModifyUnblockedDamageTarget
sts2::Creature ModifyUnblockedDamageTarget(sts2::CombatState combatState, sts2::Creature originalTarget, sts2::Dec amount, sts2::ValueProp props, sts2::Creature dealer) {
    HookExecutionScope hook_execution_observation("Hook.ModifyUnblockedDamageTarget", combatState, originalTarget, amount, props, dealer);
    return hook_execution_observation.Invoke("Creature", [&]() -> sts2::Creature {
    // STS2_EXECUTION_BODY_BEGIN

    Creature creature = originalTarget;
    for (AbstractModel item : IterateHookListenersOf(combatState)) {
        creature = item.ModifyUnblockedDamageTarget(creature, amount, props, dealer);
    }
    return creature;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"originalTarget", "Creature", 0, FreezeExecutionValue(originalTarget)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"originalTarget", "Creature", 0, FreezeExecutionValue(originalTarget)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"props", "ValueProp", 0, FreezeExecutionValue(props)}, ExecutionArgument{"dealer", "Creature", 0, FreezeExecutionValue(dealer)}}; });
}

// Hooks/Hook.cs: ModifyXValue
int32_t ModifyXValue(sts2::CombatState combatState, sts2::CardModel card, int32_t originalValue) {
    HookExecutionScope hook_execution_observation("Hook.ModifyXValue", combatState, card, originalValue);
    return hook_execution_observation.Invoke("Int32", [&]() -> int32_t {
    // STS2_EXECUTION_BODY_BEGIN

    int32_t num = originalValue;
    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        num = item.ModifyXValue(card, num);
    }
    return num;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"originalValue", "Int32", 0, FreezeExecutionValue(originalValue)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"originalValue", "Int32", 0, FreezeExecutionValue(originalValue)}}; });
}

// Hooks/Hook.cs: ShouldAddToDeck
bool ShouldAddToDeck(sts2::RunState runState, sts2::CardModel card, sts2::AbstractModel& preventer) {
    HookExecutionScope hook_execution_observation("Hook.ShouldAddToDeck", runState, card, preventer);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        if (!item.ShouldAddToDeck(card)) {
            preventer = item;
            return false;
        }
    }
    preventer = {};
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"preventer", "AbstractModel&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"preventer", "AbstractModel&", 3, FreezeExecutionValue(preventer)}}; });
}

// Hooks/Hook.cs: ShouldAfflict
bool ShouldAfflict(sts2::CombatState combatState, sts2::CardModel card, sts2::AfflictionModel affliction) {
    HookExecutionScope hook_execution_observation("Hook.ShouldAfflict", combatState, card, affliction);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldAfflict(card, affliction)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"affliction", "AfflictionModel", 0, FreezeExecutionValue(affliction)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"affliction", "AfflictionModel", 0, FreezeExecutionValue(affliction)}}; });
}

// Hooks/Hook.cs: ShouldAllowHitting
bool ShouldAllowHitting(sts2::CombatState combatState, sts2::Creature creature) {
    HookExecutionScope hook_execution_observation("Hook.ShouldAllowHitting", combatState, creature);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldAllowHitting(creature)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; });
}

// Hooks/Hook.cs: ShouldAllowMerchantCardRemoval
bool ShouldAllowMerchantCardRemoval(sts2::RunState runState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldAllowMerchantCardRemoval", runState, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        if (!item.ShouldAllowMerchantCardRemoval(player)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: AfterItemPurchased.
void AfterItemPurchased(sts2::RunState runState, sts2::Player player, sts2::Merchant::EntryRef entry,
                        int32_t goldSpent) {
    HookExecutionScope hook_execution_observation("Hook.AfterItemPurchased", runState, player, entry, goldSpent);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        item.AfterItemPurchased(player, entry, goldSpent);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"itemPurchased", "MerchantEntry", 0, FreezeExecutionValue(entry)}, ExecutionArgument{"goldSpent", "Int32", 0, FreezeExecutionValue(goldSpent)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"itemPurchased", "MerchantEntry", 0, FreezeExecutionValue(entry)}, ExecutionArgument{"goldSpent", "Int32", 0, FreezeExecutionValue(goldSpent)}}; });
}

// Hooks/Hook.cs: ShouldRefillMerchantEntry.
bool ShouldRefillMerchantEntry(sts2::RunState runState, sts2::Merchant::EntryRef entry, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldRefillMerchantEntry", runState, entry, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        if (item.ShouldRefillMerchantEntry(entry, player)) return true;
    }
    return false;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"entry", "MerchantEntry", 0, FreezeExecutionValue(entry)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"entry", "MerchantEntry", 0, FreezeExecutionValue(entry)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: ShouldAllowTargeting
bool ShouldAllowTargeting(sts2::CombatState combatState, sts2::Creature target, sts2::AbstractModel& preventer) {
    HookExecutionScope hook_execution_observation("Hook.ShouldAllowTargeting", combatState, target, preventer);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldAllowTargeting(target)) {
            preventer = item;
            return false;
        }
    }
    preventer = {};
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"preventer", "AbstractModel&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"target", "Creature", 0, FreezeExecutionValue(target)}, ExecutionArgument{"preventer", "AbstractModel&", 3, FreezeExecutionValue(preventer)}}; });
}

// Hooks/Hook.cs: ShouldClearBlock
bool ShouldClearBlock(sts2::CombatState combatState, sts2::Creature creature, sts2::AbstractModel& preventer) {
    HookExecutionScope hook_execution_observation("Hook.ShouldClearBlock", combatState, creature, preventer);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldClearBlock(creature)) {
            preventer = item;
            return false;
        }
    }
    preventer = {};
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"preventer", "AbstractModel&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"preventer", "AbstractModel&", 3, FreezeExecutionValue(preventer)}}; });
}

// Hooks/Hook.cs: ShouldCreatureBeRemovedFromCombatAfterDeath
bool ShouldCreatureBeRemovedFromCombatAfterDeath(sts2::CombatState combatState, sts2::Creature creature) {
    HookExecutionScope hook_execution_observation("Hook.ShouldCreatureBeRemovedFromCombatAfterDeath", combatState, creature);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateHookListenersOf(combatState)) {
        if (!item.ShouldCreatureBeRemovedFromCombatAfterDeath(creature)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}}; });
}

// Hooks/Hook.cs: ShouldDie
bool ShouldDie(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature creature, sts2::AbstractModel& preventer) {
    HookExecutionScope hook_execution_observation("Hook.ShouldDie", runState, combatState, creature, preventer);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, combatState)) {
        if (!item.ShouldDie(creature)) {
            preventer = item;
            return false;
        }
    }
    for (AbstractModel item2 : IterateRunHookListeners(runState, combatState)) {
        if (!item2.ShouldDieLate(creature)) {
            preventer = item2;
            return false;
        }
    }
    preventer = {};
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"preventer", "AbstractModel&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"creature", "Creature", 0, FreezeExecutionValue(creature)}, ExecutionArgument{"preventer", "AbstractModel&", 3, FreezeExecutionValue(preventer)}}; });
}

// Hooks/Hook.cs: ShouldDisableRemainingRestSiteOptions
bool ShouldDisableRemainingRestSiteOptions(sts2::RunState runState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldDisableRemainingRestSiteOptions", runState, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        if (!item.ShouldDisableRemainingRestSiteOptions(player)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: ShouldDraw
bool ShouldDraw(sts2::CombatState combatState, sts2::Player player, bool fromHandDraw, sts2::AbstractModel& modifier) {
    HookExecutionScope hook_execution_observation("Hook.ShouldDraw", combatState, player, fromHandDraw, modifier);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldDraw(player, fromHandDraw)) {
            modifier = item;
            return false;
        }
    }
    modifier = {};
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"fromHandDraw", "Boolean", 0, FreezeExecutionValue(fromHandDraw)}, ExecutionArgument{"modifier", "AbstractModel&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"fromHandDraw", "Boolean", 0, FreezeExecutionValue(fromHandDraw)}, ExecutionArgument{"modifier", "AbstractModel&", 3, FreezeExecutionValue(modifier)}}; });
}

// Hooks/Hook.cs: ShouldEtherealTrigger
bool ShouldEtherealTrigger(sts2::CombatState combatState, sts2::CardModel card) {
    HookExecutionScope hook_execution_observation("Hook.ShouldEtherealTrigger", combatState, card);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldEtherealTrigger(card)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}}; });
}

// Hooks/Hook.cs: ShouldFlush
bool ShouldFlush(sts2::CombatState combatState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldFlush", combatState, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldFlush(player)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: ShouldGenerateTreasure
bool ShouldGenerateTreasure(sts2::RunState runState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldGenerateTreasure", runState, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        if (!item.ShouldGenerateTreasure(player)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: ShouldGainStars
bool ShouldGainStars(sts2::CombatState combatState, sts2::Dec amount, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldGainStars", combatState, amount, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldGainStars(amount, player)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: ShouldPayExcessEnergyCostWithStars
bool ShouldPayExcessEnergyCostWithStars(sts2::CombatState combatState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldPayExcessEnergyCostWithStars", combatState, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (item.ShouldPayExcessEnergyCostWithStars(player)) {
            return true;
        }
    }
    return false;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: ShouldPlay
bool ShouldPlay(sts2::CombatState combatState, sts2::CardModel card, sts2::AbstractModel& preventer, sts2::AutoPlayType autoPlayType) {
    HookExecutionScope hook_execution_observation("Hook.ShouldPlay", combatState, card, preventer, autoPlayType);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldPlay(card, autoPlayType)) {
            preventer = item;
            return false;
        }
    }
    preventer = {};
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"preventer", "AbstractModel&", 3, UndefinedExecutionValue()}, ExecutionArgument{"autoPlayType", "AutoPlayType", 0, FreezeExecutionValue(autoPlayType)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"card", "CardModel", 0, FreezeExecutionValue(card)}, ExecutionArgument{"preventer", "AbstractModel&", 3, FreezeExecutionValue(preventer)}, ExecutionArgument{"autoPlayType", "AutoPlayType", 0, FreezeExecutionValue(autoPlayType)}}; });
}

// Hooks/Hook.cs: ShouldPlayerResetEnergy
bool ShouldPlayerResetEnergy(sts2::CombatState combatState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldPlayerResetEnergy", combatState, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (!item.ShouldPlayerResetEnergy(player)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: ShouldProceedToNextMapPoint
bool ShouldProceedToNextMapPoint(sts2::RunState runState) {
    HookExecutionScope hook_execution_observation("Hook.ShouldProceedToNextMapPoint", runState);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        if (!item.ShouldProceedToNextMapPoint()) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}}; });
}

// Hooks/Hook.cs: ShouldProcurePotion
bool ShouldProcurePotion(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldProcurePotion", runState, combatState, potion, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, combatState)) {
        if (!item.ShouldProcurePotion(potion, player)) {
            return false;
        }
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"potion", "PotionModel", 0, FreezeExecutionValue(potion)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: ShouldStopCombatFromEnding
bool ShouldStopCombatFromEnding(sts2::CombatState combatState) {
    HookExecutionScope hook_execution_observation("Hook.ShouldStopCombatFromEnding", combatState);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateHookListenersOf(combatState)) {
        if (item.ShouldStopCombatFromEnding()) {
            return true;
        }
    }
    return false;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}}; });
}

// Hooks/Hook.cs: ShouldTakeExtraTurn
bool ShouldTakeExtraTurn(sts2::CombatState combatState, sts2::Player player) {
    HookExecutionScope hook_execution_observation("Hook.ShouldTakeExtraTurn", combatState, player);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateCombatHookListeners(combatState)) {
        if (item.ShouldTakeExtraTurn(player)) {
            return true;
        }
    }
    return false;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}}; });
}

// Hooks/Hook.cs: ShouldForcePotionReward
bool ShouldForcePotionReward(sts2::RunState runState, sts2::Player player, sts2::RoomType roomType) {
    HookExecutionScope hook_execution_observation("Hook.ShouldForcePotionReward", runState, player, roomType);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    bool flag = false;
    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        flag = flag || item.ShouldForcePotionReward(player, roomType);
    }
    return flag;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"roomType", "RoomType", 0, FreezeExecutionValue(roomType)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"roomType", "RoomType", 0, FreezeExecutionValue(roomType)}}; });
}

// Hooks/Hook.cs: ShouldAllowFreeTravel
bool ShouldAllowFreeTravel(sts2::RunState runState) {
    HookExecutionScope hook_execution_observation("Hook.ShouldAllowFreeTravel", runState);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    bool flag = false;
    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        flag = flag || item.ShouldAllowFreeTravel();
    }
    return flag;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}}; });
}

// Hooks/Hook.cs: ShouldPowerBeRemovedOnDeath
bool ShouldPowerBeRemovedOnDeath(sts2::PowerModel power) {
    HookExecutionScope hook_execution_observation("Hook.ShouldPowerBeRemovedOnDeath", power);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    if (!power.Owner().CombatState()) return true;
    for (AbstractModel item : IterateHookListenersOf(power.CombatState())) {
        if (!item.ShouldPowerBeRemovedOnDeath(power)) return false;
    }
    return true;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"power", "PowerModel", 0, FreezeExecutionValue(power)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"power", "PowerModel", 0, FreezeExecutionValue(power)}}; });
}

// Hooks/Hook.cs: BeforeCombatRewardOffered
void BeforeCombatRewardOffered(sts2::RewardsSet rewards, sts2::RunState runState, sts2::CombatRoom room) {
    HookExecutionScope hook_execution_observation("Hook.BeforeCombatRewardOffered", rewards, runState, room);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, CombatState())) {
        model.BeforeCombatRewardOffered(rewards, room);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"rewards", "RewardsSet", 0, FreezeExecutionValue(rewards)}, ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"room", "CombatRoom", 0, FreezeExecutionValue(room)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"rewards", "RewardsSet", 0, FreezeExecutionValue(rewards)}, ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"room", "CombatRoom", 0, FreezeExecutionValue(room)}}; });
}

// Hooks/Hook.cs: AfterRewardTaken
void AfterRewardTaken(sts2::RunState runState, sts2::Player player, sts2::Reward reward) {
    HookExecutionScope hook_execution_observation("Hook.AfterRewardTaken", runState, player, reward);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel model : IterateRunHookListeners(runState, CombatState())) {
        model.AfterRewardTaken(player, reward);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"reward", "Reward", 0, FreezeExecutionValue(reward)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"reward", "Reward", 0, FreezeExecutionValue(reward)}}; });
}

// Hooks/Hook.cs: ModifyCardRewardCreationOptions
sts2::CardCreationOptionsState ModifyCardRewardCreationOptions(sts2::RunState runState, sts2::Player player, const sts2::CardCreationOptionsState& options_) {
    HookExecutionScope hook_execution_observation("Hook.ModifyCardRewardCreationOptions", runState, player, options_);
    return hook_execution_observation.Invoke("CardCreationOptions", [&]() -> sts2::CardCreationOptionsState {
    // STS2_EXECUTION_BODY_BEGIN

    sts2::CardCreationOptionsState options = options_;
    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        options = item.ModifyCardRewardCreationOptions(player, options);
    }
    for (AbstractModel item2 : IterateRunHookListeners(runState, CombatState())) {
        options = item2.ModifyCardRewardCreationOptionsLate(player, options);
    }
    return options;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"options", "CardCreationOptions", 0, FreezeExecutionValue(options_)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"options", "CardCreationOptions", 0, FreezeExecutionValue(options_)}}; });
}

// Hooks/Hook.cs: TryModifyCardRewardOptions
bool TryModifyCardRewardOptions(sts2::RunState runState, sts2::Player player, sts2::CardList& cardRewardOptions, const sts2::CardCreationOptionsState& creationOptions, sts2::ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.TryModifyCardRewardOptions", runState, player, cardRewardOptions, creationOptions, modifiers);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    bool flag = false;
    modifiers.clear();
    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        bool flag2 = item.TryModifyCardRewardOptions(player, cardRewardOptions, creationOptions);
        flag = flag || flag2;
        if (flag2) {
            modifiers.push_back(item);
        }
    }
    for (AbstractModel item2 : IterateRunHookListeners(runState, CombatState())) {
        bool flag3 = item2.TryModifyCardRewardOptionsLate(player, cardRewardOptions, creationOptions);
        flag = flag || flag3;
        if (flag3) {
            modifiers.push_back(item2);
        }
    }
    return flag;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"cardRewardOptions", "List<CardCreationResult>", 0, FreezeExecutionValue(cardRewardOptions)}, ExecutionArgument{"creationOptions", "CardCreationOptions", 0, FreezeExecutionValue(creationOptions)}, ExecutionArgument{"modifiers", "List<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"cardRewardOptions", "List<CardCreationResult>", 0, FreezeExecutionValue(cardRewardOptions)}, ExecutionArgument{"creationOptions", "CardCreationOptions", 0, FreezeExecutionValue(creationOptions)}, ExecutionArgument{"modifiers", "List<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

// Hooks/Hook.cs: ModifyMerchantCardCreationResults
void ModifyMerchantCardCreationResults(sts2::RunState runState, sts2::Player player, sts2::CardList& cards) {
    HookExecutionScope hook_execution_observation("Hook.ModifyMerchantCardCreationResults", runState, player, cards);
    return hook_execution_observation.Invoke("Void", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        item.ModifyMerchantCardCreationResults(player, cards);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"cards", "List<CardCreationResult>", 0, FreezeExecutionValue(cards)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"cards", "List<CardCreationResult>", 0, FreezeExecutionValue(cards)}}; });
}

// Hooks/Hook.cs: ModifyRestSiteHealRewards
sts2::ModelList ModifyRestSiteHealRewards(sts2::RunState runState, sts2::Player player, sts2::RewardList rewards, bool isMimicked) {
    HookExecutionScope hook_execution_observation("Hook.ModifyRestSiteHealRewards", runState, player, rewards, isMimicked);
    return hook_execution_observation.Invoke("IEnumerable<AbstractModel>", [&]() -> sts2::ModelList {
    // STS2_EXECUTION_BODY_BEGIN

    ModelList list;
    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        if (item.TryModifyRestSiteHealRewards(player, rewards, isMimicked)) {
            list.push_back(item);
        }
    }
    return list;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"rewards", "List<Reward>", 0, FreezeExecutionValue(rewards)}, ExecutionArgument{"isMimicked", "Boolean", 0, FreezeExecutionValue(isMimicked)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"rewards", "List<Reward>", 0, FreezeExecutionValue(rewards)}, ExecutionArgument{"isMimicked", "Boolean", 0, FreezeExecutionValue(isMimicked)}}; });
}

// Hooks/Hook.cs: ModifyRewards
sts2::ModelList ModifyRewards(sts2::RunState runState, sts2::Player player, sts2::RewardList rewards, sts2::AbstractRoom room) {
    HookExecutionScope hook_execution_observation("Hook.ModifyRewards", runState, player, rewards, room);
    return hook_execution_observation.Invoke("IEnumerable<AbstractModel>", [&]() -> sts2::ModelList {
    // STS2_EXECUTION_BODY_BEGIN

    ModelList list;
    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        if (item.TryModifyRewards(player, rewards, room)) {
            list.push_back(item);
        }
    }
    for (AbstractModel item2 : IterateRunHookListeners(runState, CombatState())) {
        if (item2.TryModifyRewardsLate(player, rewards, room)) {
            list.push_back(item2);
        }
    }
    return list;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"rewards", "List<Reward>", 0, FreezeExecutionValue(rewards)}, ExecutionArgument{"room", "AbstractRoom", 0, FreezeExecutionValue(room)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"rewards", "List<Reward>", 0, FreezeExecutionValue(rewards)}, ExecutionArgument{"room", "AbstractRoom", 0, FreezeExecutionValue(room)}}; });
}

// Hooks/Hook.cs: ShouldAllowSelectingMoreCardRewards
bool ShouldAllowSelectingMoreCardRewards(sts2::RunState runState, sts2::Player player, sts2::Reward reward) {
    HookExecutionScope hook_execution_observation("Hook.ShouldAllowSelectingMoreCardRewards", runState, player, reward);
    return hook_execution_observation.Invoke("Boolean", [&]() -> bool {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel item : IterateRunHookListeners(runState, CombatState())) {
        if (item.ShouldAllowSelectingMoreCardRewards(player, reward)) {
            return true;
        }
    }
    return false;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"reward", "CardReward", 0, FreezeExecutionValue(reward)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"runState", "IRunState", 0, FreezeExecutionValue(runState)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"reward", "CardReward", 0, FreezeExecutionValue(reward)}}; });
}

int32_t ModifyOrbPassiveTriggerCount(CombatState combatState, OrbModel orb, int32_t triggerCount, ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.ModifyOrbPassiveTriggerCount", combatState, orb, triggerCount, modifiers);
    return hook_execution_observation.Invoke("Int32", [&]() -> int32_t {
    // STS2_EXECUTION_BODY_BEGIN

    int32_t count = triggerCount;
    for (AbstractModel listener : IterateCombatHookListeners(combatState)) {
        int32_t previous = count;
        count = listener.ModifyOrbPassiveTriggerCounts(orb, count);
        if (count != previous) modifiers.push_back(listener);
    }
    return count;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}, ExecutionArgument{"triggerCount", "Int32", 0, FreezeExecutionValue(triggerCount)}, ExecutionArgument{"modifyingModels", "List<AbstractModel>&", 3, UndefinedExecutionValue()}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}, ExecutionArgument{"triggerCount", "Int32", 0, FreezeExecutionValue(triggerCount)}, ExecutionArgument{"modifyingModels", "List<AbstractModel>&", 3, FreezeExecutionValue(modifiers)}}; });
}

void AfterModifyingOrbPassiveTriggerCount(CombatState combatState, OrbModel orb, const ModelList& modifiers) {
    HookExecutionScope hook_execution_observation("Hook.AfterModifyingOrbPassiveTriggerCount", combatState, orb, modifiers);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel listener : IterateCombatHookListeners(combatState)) {
        if (modifiers.contains(listener)) listener.AfterModifyingOrbPassiveTriggerCount(orb);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}, ExecutionArgument{"modifiers", "IEnumerable<AbstractModel>", 0, FreezeExecutionValue(modifiers)}}; });
}

Dec ModifyOrbValue(CombatState combatState, OrbModel orb, Dec amount) {
    HookExecutionScope hook_execution_observation("Hook.ModifyOrbValue", combatState, orb, amount);
    return hook_execution_observation.Invoke("Decimal", [&]() -> Dec {
    // STS2_EXECUTION_BODY_BEGIN

    Dec value = amount;
    for (AbstractModel listener : IterateCombatHookListeners(combatState)) value = listener.ModifyOrbValue(orb, value);
    return value;
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}, ExecutionArgument{"amount", "Decimal", 0, FreezeExecutionValue(amount)}}; });
}

void AfterOrbChanneled(CombatState combatState, PlayerChoiceContext& choiceContext, Player player, OrbModel orb) {
    HookExecutionScope hook_execution_observation("Hook.AfterOrbChanneled", combatState, choiceContext, player, orb);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel listener : IterateCombatHookListeners(combatState)) {
        choiceContext.PushModel(listener);
        listener.AfterOrbChanneled(choiceContext, player, orb);
        choiceContext.PopModel(listener);
    }
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"player", "Player", 0, FreezeExecutionValue(player)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}}; });
}

void AfterOrbEvoked(PlayerChoiceContext& choiceContext, CombatState combatState, OrbModel orb, const CreatureList& targets) {
    HookExecutionScope hook_execution_observation("Hook.AfterOrbEvoked", choiceContext, combatState, orb, targets);
    return hook_execution_observation.Invoke("Task", [&]() -> void {
    // STS2_EXECUTION_BODY_BEGIN

    for (AbstractModel listener : IterateCombatHookListeners(combatState)) listener.AfterOrbEvoked(choiceContext, orb, targets);
    // STS2_EXECUTION_BODY_END
    }, [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}, ExecutionArgument{"targets", "IEnumerable<Creature>", 0, FreezeExecutionValue(targets)}}; },
       [&]() { return std::vector<ExecutionArgument>{ExecutionArgument{"choiceContext", "PlayerChoiceContext", 0, FreezeExecutionValue(choiceContext)}, ExecutionArgument{"combatState", "ICombatState", 0, FreezeExecutionValue(combatState)}, ExecutionArgument{"orb", "OrbModel", 0, FreezeExecutionValue(orb)}, ExecutionArgument{"targets", "IEnumerable<Creature>", 0, FreezeExecutionValue(targets)}}; });
}

}  // namespace Hook
}  // namespace sts2
