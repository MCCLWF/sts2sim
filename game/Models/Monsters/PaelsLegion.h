// Models/Monsters/PaelsLegion.cs (v0.111.0). Skin and animation are presentation only.
#pragma once

#include "game/Models/MonsterModel.h"

namespace sts2 {

struct PaelsLegionMonster : MonsterModel {
    SIM_MODEL(PaelsLegionMonster, MonsterModel)

    bool IsHealthBarVisible() const { return false; }
    void NothingMove(const CreatureList&) const {}
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder builder;
        MoveState nothing = builder.Move("NOTHING_MOVE", SIM_MOVE(PaelsLegionMonster, NothingMove));
        nothing.SetFollowUpState(nothing);
        builder.Add(nothing);
        return builder.Build(nothing);
    }
};

}  // namespace sts2
