#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct LivingShield : MonsterModel {
    SIM_MODEL(LivingShield, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_HasDeathSfx);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t SmashDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 18, 16);
    }

    bool HasLivingAlly() const {
        for (sts2::Creature ally : Creature().CombatState().GetTeammatesOf(Creature())) {
            if (ally != Creature() && ally.IsAlive()) return true;
        }
        return false;
    }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::RAMPART_POWER, ctx, Creature(), Dec(25), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState shieldSlam = b.Move("SHIELD_SLAM_MOVE", SIM_MOVE(LivingShield, ShieldSlamMove),
                                      SingleAttackIntent(6));
        MoveState smash = b.Move("SMASH_MOVE", SIM_MOVE(LivingShield, SmashMove),
                                 SingleAttackIntent(SmashDamage()), BuffIntent());
        ConditionalBranchState branch = b.ConditionalBranch("SHIELD_SLAM_BRANCH");
        shieldSlam.SetFollowUpState(branch);
        branch.AddState(shieldSlam, SIM_COND(LivingShield, self.HasLivingAlly()));
        branch.AddState(smash, SIM_COND(LivingShield, !self.HasLivingAlly()));
        smash.SetFollowUpState(smash);
        b.Add(shieldSlam);
        b.Add(smash);
        b.Add(branch);
        return b.Build(shieldSlam);
    }

    void ShieldSlamMove(const CreatureList&) const {
        DamageCmd::Attack(6).FromMonster(*this).Execute(nullptr);
    }

    void SmashMove(const CreatureList&) const {
        DamageCmd::Attack(SmashDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }
};

}  // namespace sts2
