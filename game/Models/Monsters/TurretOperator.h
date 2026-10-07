#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct TurretOperator : MonsterModel {
    SIM_MODEL(TurretOperator, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_HurtSfx);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t FireDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3);
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState unload1 = b.Move("UNLOAD_MOVE", SIM_MOVE(TurretOperator, UnloadMove),
                                   MultiAttackIntent(FireDamage(), 5));
        MoveState unload2 = b.Move("UNLOAD_MOVE_2", SIM_MOVE(TurretOperator, UnloadMove),
                                   MultiAttackIntent(FireDamage(), 5));
        MoveState reload = b.Move("RELOAD_MOVE", SIM_MOVE(TurretOperator, ReloadMove), BuffIntent());
        unload1.SetFollowUpState(unload2);
        unload2.SetFollowUpState(reload);
        reload.SetFollowUpState(unload1);
        b.Add(unload1);
        b.Add(unload2);
        b.Add(reload);
        return b.Build(unload1);
    }

    void UnloadMove(const CreatureList&) const {
        DamageCmd::Attack(FireDamage()).WithHitCount(5).FromMonster(*this).Execute(nullptr);
    }

    void ReloadMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }
};

}  // namespace sts2
