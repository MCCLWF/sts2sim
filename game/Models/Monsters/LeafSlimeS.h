// Models/Monsters/LeafSlimeS.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct LeafSlimeS : MonsterModel {
    SIM_MODEL(LeafSlimeS, MonsterModel)

    int32_t TackleDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("TACKLE_MOVE", SIM_MOVE(LeafSlimeS, TackleMove), SingleAttackIntent(TackleDamage()));
        MoveState moveState2 = b.Move("GOOP_MOVE", SIM_MOVE(LeafSlimeS, GoopMove), StatusIntent(1));
        RandomBranchState randomBranchState = b.RandomBranch("RAND");
        moveState.SetFollowUpState(randomBranchState);
        moveState2.SetFollowUpState(randomBranchState);
        randomBranchState.AddBranch(moveState, MoveRepeatType::CannotRepeat);
        randomBranchState.AddBranch(moveState2, MoveRepeatType::CannotRepeat);
        b.Add(moveState);
        b.Add(moveState2);
        b.Add(randomBranchState);
        return b.Build(randomBranchState);
    }

    void TackleMove(const CreatureList& targets) const { DamageCmd::Attack(TackleDamage()).FromMonster(*this).Execute(nullptr); }
    void GoopMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::SLIMED, targets, PileType::Discard, 1, Player{});
    }
};

}  // namespace sts2
