// Commands/Builders/AttackContext.cs
#pragma once

#include <exception>
#include <utility>

#include "game/Commands/Builders/AttackCommand.h"
#include "game/Hooks/Hook.h"

namespace sts2 {

// Groups direct CreatureCmd::Damage calls under the source game's one attack
// hook lifecycle, without executing AttackCommand::Execute (which would also
// add a creature-attack history/count entry).
class AttackContext {
public:
    AttackContext(CombatState combatState, PlayerChoiceContext& choiceContext,
                  CardModel card, const CardPlay* cardPlay)
        : combatState_(combatState), choiceContext_(&choiceContext), command_(Dec(0)) {
        if (!cardPlay) throw GameError("cardPlay is null");
        command_.FromCard(card, cardPlay).TargetingAllOpponents(combatState_);
        Hook::BeforeAttack(combatState_, command_);
    }

    void AddHit(const DamageResults& results) {
        command_.IncrementHitsInternal();
        command_.AddResultsInternal(results);
    }

    void Finish() {
        if (finished_) return;
        finished_ = true;
        Hook::AfterAttack(combatState_, *choiceContext_, command_);
    }

    // A choice suspends the source async method with its context still open.
    // Let NeedChoice escape unchanged; Env will restore and replay this prefix
    // after the answer. On every normally-completed path, Finish runs once.
    template <class Action>
    void Run(Action&& action) {
        try {
            std::forward<Action>(action)();
        } catch (const NeedChoice&) {
            // This is a suspended source async method. Keep its state open so
            // Env can expose the request and replay the prefix after an answer.
            throw;
        } catch (...) {
            std::exception_ptr bodyFailure = std::current_exception();
            // Match await-using cleanup for ordinary failures. If a hook also
            // fails, let that hook error propagate rather than hiding it.
            Finish();
            std::rethrow_exception(bodyFailure);
        }
        Finish();
    }

private:
    CombatState combatState_;
    PlayerChoiceContext* choiceContext_;
    AttackCommand command_;
    bool finished_ = false;
};

}  // namespace sts2
