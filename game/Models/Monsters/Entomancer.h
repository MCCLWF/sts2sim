// Models/Monsters/Entomancer.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Entomancer : MonsterModel {
    SIM_MODEL(Entomancer, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t SpearMoveDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 20, 18); }
    int32_t BeesRepeat() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::PERSONAL_HIVE_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState spit = b.Move("PHEROMONE_SPIT_MOVE", SIM_MOVE(Entomancer, SpitMove), BuffIntent());
        MoveState bees = b.Move("BEES_MOVE", SIM_MOVE(Entomancer, BeesMove), MultiAttackIntent(3, BeesRepeat()));
        MoveState spear = b.Move("SPEAR_MOVE", SIM_MOVE(Entomancer, SpearMove), SingleAttackIntent(SpearMoveDamage()));
        spit.SetFollowUpState(bees);
        bees.SetFollowUpState(spear);
        spear.SetFollowUpState(spit);
        b.Add(spit);
        b.Add(bees);
        b.Add(spear);
        return b.Build(bees);
    }

    void SpitMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        if (Creature().GetPowerAmount(PowerId::PERSONAL_HIVE_POWER) >= 3) {
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
            return;
        }
        PowerCmd::Apply(PowerId::PERSONAL_HIVE_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }
    void BeesMove(const CreatureList&) const {
        DamageCmd::Attack(3).WithHitCount(BeesRepeat()).FromMonster(*this).Execute(nullptr);
    }
    void SpearMove(const CreatureList&) const {
        DamageCmd::Attack(SpearMoveDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
