// Models/Monsters/TwigSlimeS.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct TwigSlimeS : MonsterModel {
    SIM_MODEL(TwigSlimeS, MonsterModel)

    int32_t TackleDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 4); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("TACKLE_MOVE", SIM_MOVE(TwigSlimeS, TackleMove), SingleAttackIntent(TackleDamage()));
        moveState.SetFollowUpState(moveState);
        b.Add(moveState);
        return b.Build(moveState);
    }

    void TackleMove(const CreatureList& targets) const { DamageCmd::Attack(TackleDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
