#pragma once

#include "game/Models/MonsterModel.h"

namespace sts2 {

struct Osty : MonsterModel {
    SIM_MODEL(Osty, MonsterModel)
    bool IsHealthBarVisible() const { return Creature().IsAlive(); }
    void NothingMove(const CreatureList&) const {}
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState nothing = b.Move("NOTHING_MOVE", SIM_MOVE(Osty, NothingMove));
        nothing.SetFollowUpState(nothing);
        b.Add(nothing);
        return b.Build(nothing);
    }
};

}  // namespace sts2
