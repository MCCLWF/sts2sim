// Models/Monsters/FatGremlin.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct FatGremlin : MonsterModel {
    SIM_MODEL(FatGremlin, MonsterModel)

    bool IsAwake() const { return extra(0) != 0; }
    void SetIsAwake(bool value) const { extra(0) = value; }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState spawned = b.Move("SPAWNED_MOVE", SIM_MOVE(FatGremlin, SpawnedMove), StunIntent());
        MoveState flee = b.Move("FLEE_MOVE", SIM_MOVE(FatGremlin, FleeMove), EscapeIntent());
        spawned.SetFollowUpState(flee);
        flee.SetFollowUpState(flee);
        b.Add(spawned);
        b.Add(flee);
        return b.Build(spawned);
    }

    void SpawnedMove(const CreatureList&) const { SetIsAwake(true); }
    void FleeMove(const CreatureList&) const { CreatureCmd::Escape(Creature()); }
};

}  // namespace sts2
