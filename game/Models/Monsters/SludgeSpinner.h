// Models/Monsters/SludgeSpinner.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SludgeSpinner : MonsterModel {
    SIM_MODEL(SludgeSpinner, MonsterModel)

    int32_t OilSprayDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t SlamDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 12, 11); }
    int32_t RageDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("OIL_SPRAY_MOVE", SIM_MOVE(SludgeSpinner, OilSprayMove), SingleAttackIntent(OilSprayDamage()), DebuffIntent());
        MoveState moveState2 = b.Move("SLAM_MOVE", SIM_MOVE(SludgeSpinner, SlamMove), SingleAttackIntent(SlamDamage()));
        MoveState moveState3 = b.Move("RAGE_MOVE", SIM_MOVE(SludgeSpinner, RageMove), SingleAttackIntent(RageDamage()), BuffIntent());
        RandomBranchState randomBranchState = b.RandomBranch("RAND");
        moveState.SetFollowUpState(randomBranchState);
        moveState2.SetFollowUpState(randomBranchState);
        moveState3.SetFollowUpState(randomBranchState);
        randomBranchState.AddBranch(moveState, MoveRepeatType::CannotRepeat);
        randomBranchState.AddBranch(moveState2, MoveRepeatType::CannotRepeat);
        randomBranchState.AddBranch(moveState3, MoveRepeatType::CannotRepeat);
        b.Add(randomBranchState);
        b.Add(moveState);
        b.Add(moveState2);
        b.Add(moveState3);
        return b.Build(moveState);
    }

    void OilSprayMove(const CreatureList& targets) const {
        DamageCmd::Attack(OilSprayDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
    void SlamMove(const CreatureList& targets) const { DamageCmd::Attack(SlamDamage()).FromMonster(*this).Execute(nullptr); }
    void RageMove(const CreatureList& targets) const {
        DamageCmd::Attack(RageDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }
};

}  // namespace sts2
