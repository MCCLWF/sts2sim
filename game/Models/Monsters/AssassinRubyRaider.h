// Models/Monsters/AssassinRubyRaider.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct AssassinRubyRaider : MonsterModel {
    SIM_MODEL(AssassinRubyRaider, MonsterModel)

    int32_t KillshotDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 10); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState killshot = b.Move("KILLSHOT_MOVE", SIM_MOVE(AssassinRubyRaider, KillshotMove), SingleAttackIntent(KillshotDamage()));
        killshot.SetFollowUpState(killshot);
        b.Add(killshot);
        return b.Build(killshot);
    }

    void KillshotMove(const CreatureList& targets) const { DamageCmd::Attack(KillshotDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
