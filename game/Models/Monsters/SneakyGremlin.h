// Models/Monsters/SneakyGremlin.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SneakyGremlin : MonsterModel {
    SIM_MODEL(SneakyGremlin, MonsterModel)

    int32_t TackleDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 10, 9); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState spawned = b.Move("SPAWNED_MOVE", SIM_MOVE(SneakyGremlin, SpawnedMove), StunIntent());
        MoveState tackle = b.Move("TACKLE_MOVE", SIM_MOVE(SneakyGremlin, TackleMove), SingleAttackIntent(TackleDamage()));
        spawned.SetFollowUpState(tackle);
        tackle.SetFollowUpState(tackle);
        b.Add(spawned);
        b.Add(tackle);
        return b.Build(spawned);
    }

    void SpawnedMove(const CreatureList&) const {}
    void TackleMove(const CreatureList&) const { DamageCmd::Attack(TackleDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
