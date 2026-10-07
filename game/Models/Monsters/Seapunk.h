// Models/Monsters/Seapunk.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Seapunk : MonsterModel {
    SIM_MODEL(Seapunk, MonsterModel)

    int32_t SeaKickDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 13, 11); }
    int32_t SpinningKickDamage() const { return 2; }
    int32_t SpinningKickRepeat() const { return 4; }
    int32_t BubbleBlock() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 8, 7); }
    int32_t BubbleStr() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 2, 1); }

    void AfterAddedToRoom() const {}  // base.AfterAddedToRoom() (nothing), then visuals

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("SEA_KICK_MOVE", SIM_MOVE(Seapunk, SeaKickMove), SingleAttackIntent(SeaKickDamage()));
        MoveState moveState2 = b.Move("SPINNING_KICK_MOVE", SIM_MOVE(Seapunk, SpinningKickMove),
                                      MultiAttackIntent(SpinningKickDamage(), SpinningKickRepeat()));
        MoveState moveState3 = b.Move("BUBBLE_BURP_MOVE", SIM_MOVE(Seapunk, BubbleBurpMove), BuffIntent(), DefendIntent());
        moveState.SetFollowUpState(moveState2);
        moveState2.SetFollowUpState(moveState3);
        moveState3.SetFollowUpState(moveState);
        b.Add(moveState);
        b.Add(moveState2);
        b.Add(moveState3);
        return b.Build(moveState);
    }

    void SeaKickMove(const CreatureList& targets) const { DamageCmd::Attack(SeaKickDamage()).FromMonster(*this).Execute(nullptr); }
    void SpinningKickMove(const CreatureList& targets) const {
        DamageCmd::Attack(SpinningKickDamage()).WithHitCount(SpinningKickRepeat()).FromMonster(*this).Execute(nullptr);
    }
    void BubbleBurpMove(const CreatureList& targets) const {
        CreatureCmd::GainBlock(Creature(), Dec(BubbleBlock()), ValueProp::Move, nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(BubbleStr()), Creature(), CardModel{});
    }
};

}  // namespace sts2
