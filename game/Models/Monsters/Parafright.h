// Models/Monsters/Parafright.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/IllusionPower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Parafright : MonsterModel {
    SIM_MODEL(Parafright, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t SlamDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 17, 16);
    }

    bool ShouldDisappearFromDoom() const { return false; }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::ILLUSION_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    void SlamMove(const CreatureList&) const { DamageCmd::Attack(SlamDamage()).FromMonster(*this).Execute(nullptr); }
    void ReviveMove(const CreatureList&) const {
        IllusionPower power(Creature().GetPower(PowerId::ILLUSION_POWER));
        power.Revive();
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState slam = b.Move("SLAM_MOVE", SIM_MOVE(Parafright, SlamMove), SingleAttackIntent(SlamDamage()));
        slam.SetFollowUpState(slam);
        MoveState revive = b.Move("REVIVE_MOVE", SIM_MOVE(Parafright, ReviveMove), HealIntent());
        revive.SetFollowUpState(slam);
        revive.SetMustPerformOnceBeforeTransitioning(true);
        b.Add(slam);
        b.Add(revive);
        return b.Build(slam);
    }
};

}  // namespace sts2
