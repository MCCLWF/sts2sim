// Models/Monsters/Guardbot.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Guardbot : MonsterModel {
    SIM_MODEL(Guardbot, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    void AfterAddedToRoom() const {}
    void GuardMove(const CreatureList&) const {
        for (sts2::Creature ally : CombatState().Enemies()) {
            if (ally.Monster().MonsterId_() == MonsterId::FABRICATOR)
                CreatureCmd::GainBlock(ally, Dec(15), ValueProp::Unpowered, nullptr);
        }
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState guard = b.Move("GUARD_MOVE", SIM_MOVE(Guardbot, GuardMove), DefendIntent());
        guard.SetFollowUpState(guard);
        b.Add(guard);
        return b.Build(guard);
    }
};

}  // namespace sts2
