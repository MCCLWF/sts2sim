// Models/Monsters/LagavulinMatriarch.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct LagavulinMatriarch : MonsterModel {
    SIM_MODEL(LagavulinMatriarch, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t SlashDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 21, 19);
    }
    int32_t Slash2Damage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 14, 12);
    }
    int32_t Slash2Block() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 14, 12);
    }
    int32_t DisembowelDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 10, 9);
    }

    // The original AfterDamageReceived and AfterDeath overrides only select
    // animation tracks and stop a visual sleep effect.
    void AfterDamageReceived(PlayerChoiceContext&, sts2::Creature, const DamageResult&,
                             ValueProp, sts2::Creature, CardModel) const {}
    void AfterDeath(PlayerChoiceContext&, sts2::Creature, bool, float) const {}

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::PLATING_POWER, ctx, Creature(), Dec(12), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::ASLEEP_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState sleep = b.Move("SLEEP_MOVE", SIM_MOVE(LagavulinMatriarch, SleepMove), SleepIntent());
        MoveState slash = b.Move("SLASH_MOVE", SIM_MOVE(LagavulinMatriarch, SlashMove),
                                 SingleAttackIntent(SlashDamage()));
        MoveState slash2 = b.Move("SLASH2_MOVE", SIM_MOVE(LagavulinMatriarch, Slash2Move),
                                  SingleAttackIntent(Slash2Damage()), DefendIntent());
        MoveState disembowel = b.Move("DISEMBOWEL_MOVE", SIM_MOVE(LagavulinMatriarch, DisembowelMove),
                                      MultiAttackIntent(DisembowelDamage(), 2));
        MoveState siphon = b.Move("SOUL_SIPHON_MOVE", SIM_MOVE(LagavulinMatriarch, SoulSiphonMove),
                                  DebuffIntent(), BuffIntent());
        ConditionalBranchState sleepBranch = b.ConditionalBranch("SLEEP_BRANCH");
        sleepBranch.AddState(sleep, SIM_COND(LagavulinMatriarch, self.Creature().HasPower(PowerId::ASLEEP_POWER)));
        sleepBranch.AddState(slash, SIM_COND(LagavulinMatriarch, !self.Creature().HasPower(PowerId::ASLEEP_POWER)));
        sleep.SetFollowUpState(sleepBranch);
        slash.SetFollowUpState(disembowel);
        disembowel.SetFollowUpState(slash2);
        slash2.SetFollowUpState(siphon);
        siphon.SetFollowUpState(slash);
        b.Add(sleepBranch);
        b.Add(sleep);
        b.Add(slash);
        b.Add(slash2);
        b.Add(disembowel);
        b.Add(siphon);
        return b.Build(sleep);
    }

    void SleepMove(const CreatureList&) const {}
    void WakeUpMove(const CreatureList&) const {}
    void SlashMove(const CreatureList&) const { DamageCmd::Attack(SlashDamage()).FromMonster(*this).Execute(nullptr); }
    void Slash2Move(const CreatureList&) const {
        DamageCmd::Attack(Slash2Damage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(Slash2Block()), ValueProp::Move, nullptr);
    }
    void DisembowelMove(const CreatureList&) const {
        DamageCmd::Attack(DisembowelDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
    }
    void SoulSiphonMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, targets, Dec(-2), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, targets, Dec(-2), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
