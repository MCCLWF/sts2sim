// Models/Monsters/ShrinkerBeetle.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct ShrinkerBeetle : MonsterModel {
    SIM_MODEL(ShrinkerBeetle, MonsterModel)

    int32_t ChompDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }
    int32_t StompDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 14, 13); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("SHRINKER_MOVE", SIM_MOVE(ShrinkerBeetle, ShrinkMove), DebuffIntent(/*strong=*/true));
        MoveState moveState2 = b.Move("CHOMP_MOVE", SIM_MOVE(ShrinkerBeetle, ChompMove), SingleAttackIntent(ChompDamage()));
        MoveState moveState3 = b.Move("STOMP_MOVE", SIM_MOVE(ShrinkerBeetle, StompMove), SingleAttackIntent(StompDamage()));
        moveState.SetFollowUpState(moveState2);
        moveState2.SetFollowUpState(moveState3);
        moveState3.SetFollowUpState(moveState2);
        b.Add(moveState);
        b.Add(moveState2);
        b.Add(moveState3);
        return b.Build(moveState);
    }

    void ShrinkMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::SHRINK_POWER, ctx, targets, Dec(-1), Creature(), CardModel{});
    }
    void ChompMove(const CreatureList& targets) const { DamageCmd::Attack(ChompDamage()).FromMonster(*this).Execute(nullptr); }
    void StompMove(const CreatureList& targets) const { DamageCmd::Attack(StompDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
