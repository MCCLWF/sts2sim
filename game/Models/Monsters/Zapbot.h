// Models/Monsters/Zapbot.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Zapbot : MonsterModel {
    SIM_MODEL(Zapbot, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t ZapDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 15, 14); }
    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::HIGH_VOLTAGE_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
    void ZapMove(const CreatureList&) const { DamageCmd::Attack(ZapDamage()).FromMonster(*this).Execute(nullptr); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState zap = b.Move("ZAP", SIM_MOVE(Zapbot, ZapMove), SingleAttackIntent(ZapDamage()));
        zap.SetFollowUpState(zap);
        b.Add(zap);
        return b.Build(zap);
    }
};

}  // namespace sts2
