// Models/Monsters/VineShambler.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct VineShambler : MonsterModel {
    SIM_MODEL(VineShambler, MonsterModel)

    int32_t GraspingVinesDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }
    int32_t SwipeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }
    int32_t ChompDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 18, 16); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState vines = b.Move("GRASPING_VINES_MOVE", SIM_MOVE(VineShambler, GraspingVinesMove),
                                 SingleAttackIntent(GraspingVinesDamage()), CardDebuffIntent());
        MoveState swipe = b.Move("SWIPE_MOVE", SIM_MOVE(VineShambler, SwipeMove), MultiAttackIntent(SwipeDamage(), 2));
        MoveState chomp = b.Move("CHOMP_MOVE", SIM_MOVE(VineShambler, ChompMove), SingleAttackIntent(ChompDamage()));
        swipe.SetFollowUpState(vines);
        vines.SetFollowUpState(chomp);
        chomp.SetFollowUpState(swipe);
        b.Add(vines); b.Add(swipe); b.Add(chomp);
        return b.Build(swipe);
    }

    void GraspingVinesMove(const CreatureList& targets) const {
        DamageCmd::Attack(GraspingVinesDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::TANGLED_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }

    void SwipeMove(const CreatureList&) const {
        DamageCmd::Attack(SwipeDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
    }

    void ChompMove(const CreatureList&) const {
        DamageCmd::Attack(ChompDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
