// Models/Monsters/Byrdpip.cs
#pragma once

#include "game/Models/MonsterModel.h"

namespace sts2 {

struct ByrdpipMonster : MonsterModel {
    SIM_MODEL(ByrdpipMonster, MonsterModel)
    bool IsHealthBarVisible() const { return false; }
    void NothingMove(const CreatureList&) const {}
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState nothing = b.Move("NOTHING_MOVE", SIM_MOVE(ByrdpipMonster, NothingMove));
        nothing.SetFollowUpState(nothing);
        b.Add(nothing);
        return b.Build(nothing);
    }
};

}  // namespace sts2
