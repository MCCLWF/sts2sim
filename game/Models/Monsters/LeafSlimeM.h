// Models/Monsters/LeafSlimeM.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct LeafSlimeM : MonsterModel {
    SIM_MODEL(LeafSlimeM, MonsterModel)

    int32_t ClumpDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("CLUMP_SHOT", SIM_MOVE(LeafSlimeM, ClumpShotMove), SingleAttackIntent(ClumpDamage()));
        MoveState moveState2 = b.Move("STICKY_SHOT", SIM_MOVE(LeafSlimeM, StickyShotMove), StatusIntent(2));
        moveState2.SetFollowUpState(moveState);
        moveState.SetFollowUpState(moveState2);
        b.Add(moveState);
        b.Add(moveState2);
        return b.Build(moveState2);
    }

    void ClumpShotMove(const CreatureList& targets) const { DamageCmd::Attack(ClumpDamage()).FromMonster(*this).Execute(nullptr); }
    void StickyShotMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::SLIMED, targets, PileType::Discard, 2, Player{});
    }
};

}  // namespace sts2
