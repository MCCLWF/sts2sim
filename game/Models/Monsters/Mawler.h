// Models/Monsters/Mawler.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Mawler : MonsterModel {
    SIM_MODEL(Mawler, MonsterModel)

    int32_t RipAndTearDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }
    int32_t ClawDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 4); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState rip = b.Move("RIP_AND_TEAR_MOVE", SIM_MOVE(Mawler, RipAndTearMove), SingleAttackIntent(RipAndTearDamage()));
        MoveState roar = b.Move("ROAR_MOVE", SIM_MOVE(Mawler, RoarMove), DebuffIntent());
        MoveState claw = b.Move("CLAW_MOVE", SIM_MOVE(Mawler, ClawMove), MultiAttackIntent(ClawDamage(), 2));
        RandomBranchState random = b.RandomBranch("RAND");
        claw.SetFollowUpState(random);
        roar.SetFollowUpState(random);
        rip.SetFollowUpState(random);
        random.AddBranch(rip, MoveRepeatType::CannotRepeat, 1.0f);
        random.AddBranch(roar, MoveRepeatType::UseOnlyOnce, 1.0f);
        random.AddBranch(claw, MoveRepeatType::CannotRepeat, 1.0f);
        b.Add(rip);
        b.Add(roar);
        b.Add(claw);
        b.Add(random);
        return b.Build(claw);
    }

    void RipAndTearMove(const CreatureList& targets) const { DamageCmd::Attack(RipAndTearDamage()).FromMonster(*this).Execute(nullptr); }
    void RoarMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, targets, Dec(3), Creature(), CardModel{});
    }
    void ClawMove(const CreatureList& targets) const { DamageCmd::Attack(ClawDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
