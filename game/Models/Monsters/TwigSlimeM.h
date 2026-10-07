// Models/Monsters/TwigSlimeM.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct TwigSlimeM : MonsterModel {
    SIM_MODEL(TwigSlimeM, MonsterModel)

    int32_t ClumpDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 12, 11); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("POKEY_POUNCE_MOVE", SIM_MOVE(TwigSlimeM, ClumpShotMove), SingleAttackIntent(ClumpDamage()));
        MoveState moveState2 = b.Move("STICKY_SHOT_MOVE", SIM_MOVE(TwigSlimeM, StickyShotMove), StatusIntent(1));
        RandomBranchState randomBranchState = b.RandomBranch("RAND");
        moveState.SetFollowUpState(randomBranchState);
        moveState2.SetFollowUpState(randomBranchState);
        randomBranchState.AddBranch(moveState, 2);
        randomBranchState.AddBranch(moveState2, MoveRepeatType::CannotRepeat);
        b.Add(moveState);
        b.Add(moveState2);
        b.Add(randomBranchState);
        return b.Build(moveState2);
    }

    void ClumpShotMove(const CreatureList& targets) const { DamageCmd::Attack(ClumpDamage()).FromMonster(*this).Execute(nullptr); }
    void StickyShotMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::SLIMED, targets, PileType::Discard, 1, Player{});
    }
};

}  // namespace sts2
