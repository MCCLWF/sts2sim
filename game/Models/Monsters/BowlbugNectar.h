#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct BowlbugNectar : MonsterModel {
    SIM_MODEL(BowlbugNectar, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(SetupSkins);
    SIM_NOT_PORTED(get_DeathSfx);
    SIM_NOT_PORTED(get_MinInitialHp);
    SIM_NOT_PORTED(get_MaxInitialHp);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t BuffStrengthGain() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 15); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState thrash = b.Move("THRASH_MOVE", SIM_MOVE(BowlbugNectar, ThrashMove), SingleAttackIntent(3));
        MoveState buff = b.Move("BUFF_MOVE", SIM_MOVE(BowlbugNectar, BuffMove), BuffIntent());
        MoveState thrash2 = b.Move("THRASH2_MOVE", SIM_MOVE(BowlbugNectar, ThrashMove), SingleAttackIntent(3));
        thrash.SetFollowUpState(buff);
        buff.SetFollowUpState(thrash2);
        thrash2.SetFollowUpState(thrash2);
        b.Add(buff);
        b.Add(thrash2);
        b.Add(thrash);
        return b.Build(thrash);
    }

    void ThrashMove(const CreatureList&) const { DamageCmd::Attack(3).FromMonster(*this).Execute(nullptr); }
    void BuffMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(BuffStrengthGain()), Creature(), CardModel{});
    }
};

}  // namespace sts2
