// Models/Monsters/TheLost.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct TheLost : MonsterModel {
    SIM_MODEL(TheLost, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t EyeLasersDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 4); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::POSSESS_STRENGTH_POWER, ctx, Creature(), Dec(1), sts2::Creature{}, CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState smog = b.Move("DEBILITATING_SMOG", SIM_MOVE(TheLost, DebilitatingSmogMove), DebuffIntent(), BuffIntent());
        MoveState lasers = b.Move("EYE_LASERS", SIM_MOVE(TheLost, EyeLasersMove), MultiAttackIntent(EyeLasersDamage(), 2));
        smog.SetFollowUpState(lasers);
        lasers.SetFollowUpState(smog);
        b.Add(smog);
        b.Add(lasers);
        return b.Build(smog);
    }

    void DebilitatingSmogMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, targets, Dec(-2), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
    void EyeLasersMove(const CreatureList&) const {
        DamageCmd::Attack(EyeLasersDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
