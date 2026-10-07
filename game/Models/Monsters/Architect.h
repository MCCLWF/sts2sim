// Models/Monsters/Architect.cs
#pragma once

#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Architect : MonsterModel {
    SIM_MODEL(Architect, MonsterModel)

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState nothing = b.Move("NOTHING", SIM_MOVE(Architect, NothingMove), HiddenIntent());
        nothing.SetFollowUpState(nothing);
        b.Add(nothing);
        return b.Build(nothing);
    }

    void NothingMove(const CreatureList&) const {}
};

}  // namespace sts2
