#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SlumberingBeetle : MonsterModel {
    SIM_MODEL(SlumberingBeetle, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    bool IsAwake() const { return extra(0) != 0; }
    void SetIsAwake(bool value) const { extra(0) = value; }
    int32_t RolloutDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 18, 16); }
    int32_t PlatingAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 18, 15); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::PLATING_POWER, ctx, Creature(), Dec(PlatingAmount()), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::SLUMBER_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState snore = b.Move("SNORE_MOVE", SIM_MOVE(SlumberingBeetle, SnoreMove), SleepIntent());
        MoveState rollout = b.Move("ROLL_OUT_MOVE", SIM_MOVE(SlumberingBeetle, RolloutMove), SingleAttackIntent(RolloutDamage()), BuffIntent());
        ConditionalBranchState next = b.ConditionalBranch("SNORE_NEXT");
        next.AddState(snore, SIM_COND(SlumberingBeetle, self.Creature().HasPower(PowerId::SLUMBER_POWER)));
        next.AddState(rollout, SIM_COND(SlumberingBeetle, !self.Creature().HasPower(PowerId::SLUMBER_POWER)));
        snore.SetFollowUpState(next);
        rollout.SetFollowUpState(rollout);
        b.Add(snore);
        b.Add(next);
        b.Add(rollout);
        return b.Build(snore);
    }

    void SnoreMove(const CreatureList&) const {}
    void WakeUpMove(const CreatureList&) const {
        SetIsAwake(true);
        PowerModel plating = Creature().GetPower(PowerId::PLATING_POWER);
        if (plating) PowerCmd::Remove(plating);
    }
    void RolloutMove(const CreatureList&) const {
        DamageCmd::Attack(RolloutDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
