// Models/Monsters/Fogmog.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/IllusionPower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Fogmog : MonsterModel {
    SIM_MODEL(Fogmog, MonsterModel)

    int32_t SwipeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t HeadbuttDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState illusion = b.Move("ILLUSION_MOVE", SIM_MOVE(Fogmog, IllusionMove), SummonIntent());
        MoveState swipe = b.Move("SWIPE_MOVE", SIM_MOVE(Fogmog, SwipeMove), SingleAttackIntent(SwipeDamage()), BuffIntent());
        MoveState swipeRandom = b.Move("SWIPE_RANDOM_MOVE", SIM_MOVE(Fogmog, SwipeMove), SingleAttackIntent(SwipeDamage()), BuffIntent());
        MoveState headbutt = b.Move("HEADBUTT_MOVE", SIM_MOVE(Fogmog, HeadbuttMove), SingleAttackIntent(HeadbuttDamage()));
        MoveState revive = b.Move("REVIVE_MOVE", SIM_MOVE(Fogmog, ReviveMove), HealIntent());
        RandomBranchState branch = b.RandomBranch("BRANCH");
        branch.AddBranch(swipeRandom, MoveRepeatType::CannotRepeat, 0.4f);
        branch.AddBranch(headbutt, MoveRepeatType::CannotRepeat, 0.6f);
        illusion.SetFollowUpState(swipe);
        swipe.SetFollowUpState(branch);
        swipeRandom.SetFollowUpState(headbutt);
        headbutt.SetFollowUpState(swipe);
        revive.SetFollowUpState(swipe);
        revive.SetMustPerformOnceBeforeTransitioning(true);
        b.Add(illusion); b.Add(swipe); b.Add(swipeRandom); b.Add(branch); b.Add(headbutt); b.Add(revive);
        return b.Build(illusion);
    }

    void IllusionMove(const CreatureList&) const {
        CreatureCmd::Add(MonsterModel::ToMutable(sim_, MonsterId::EYE_WITH_TEETH), CombatState(), CombatSide::Enemy, "illusion");
    }

    void SwipeMove(const CreatureList&) const {
        DamageCmd::Attack(SwipeDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    void HeadbuttMove(const CreatureList&) const {
        DamageCmd::Attack(HeadbuttDamage()).FromMonster(*this).Execute(nullptr);
    }

    void ReviveMove(const CreatureList&) const {
        IllusionPower power(Creature().GetPower(PowerId::ILLUSION_POWER));
        power.Revive();
    }
};

}  // namespace sts2
