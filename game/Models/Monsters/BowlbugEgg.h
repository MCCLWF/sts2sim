#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct BowlbugEgg : MonsterModel {
    SIM_MODEL(BowlbugEgg, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(SetupSkins);
    SIM_NOT_PORTED(get_AttackSfx);
    SIM_NOT_PORTED(get_DeathSfx);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    void AfterAddedToRoom() const {}

    int32_t BiteDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }
    int32_t ProtectBlock() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState bite = b.Move("BITE_MOVE", SIM_MOVE(BowlbugEgg, BiteMove), SingleAttackIntent(BiteDamage()), DefendIntent());
        bite.SetFollowUpState(bite);
        b.Add(bite);
        return b.Build(bite);
    }

    void BiteMove(const CreatureList&) const {
        DamageCmd::Attack(BiteDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(ProtectBlock()), ValueProp::Move, nullptr);
    }
};

}  // namespace sts2
