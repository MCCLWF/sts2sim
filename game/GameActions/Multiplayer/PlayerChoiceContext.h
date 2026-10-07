// GameActions/Multiplayer/PlayerChoiceContext.cs (and its subclasses).
// In single player the context only tracks which models are acting (the
// model stack); the choice itself goes through Sim::Choose.
#pragma once

#include "engine/containers.h"
#include "game/Models/AbstractModel.h"

namespace sts2 {

class PlayerChoiceContext {
public:
    // ThrowingPlayerChoiceContext: used where no player choice may happen
    // (monster moves); a choice there is a bug.
    static PlayerChoiceContext Throwing() {
        PlayerChoiceContext c;
        c.throwing_ = true;
        return c;
    }

    AbstractModel LastInvolvedModel() const { return models_.empty() ? AbstractModel{} : models_.back(); }
    void PushModel(AbstractModel model) { models_.push_back(model); }
    void PopModel(AbstractModel model) {
        if (!models_.empty() && models_.back() == model) models_.pop_back();
    }
    bool IsThrowing() const { return throwing_; }

private:
    friend ExecutionValue FreezeExecutionValue(const PlayerChoiceContext&);
    StaticVec<AbstractModel, 16> models_;
    bool throwing_ = false;
};

}  // namespace sts2
