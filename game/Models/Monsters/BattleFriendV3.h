// Models/Monsters/BattleFriendV3.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct BattleFriendV3 : MonsterModel {
    SIM_MODEL(BattleFriendV3, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(SetupSkins);

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState wait = b.Move("NOTHING_MOVE", SIM_MOVE(BattleFriendV3, NothingMove));
        wait.SetFollowUpState(wait);
        b.Add(wait);
        return b.Build(wait);
    }

    void NothingMove(const CreatureList&) const {}

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::BATTLEWORN_DUMMY_TIME_LIMIT_POWER, ctx, Creature(), Dec(3), sts2::Creature{}, CardModel{});  // applier: null
    }
};

}  // namespace sts2
