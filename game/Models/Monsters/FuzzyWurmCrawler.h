// Models/Monsters/FuzzyWurmCrawler.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct FuzzyWurmCrawler : MonsterModel {
    SIM_MODEL(FuzzyWurmCrawler, MonsterModel)

    int32_t AcidGoopDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 4); }
    bool IsPuffed() const { return extra(0) != 0; }  // _isPuffed
    void SetIsPuffed(bool value) const { extra(0) = value; }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("FIRST_ACID_GOOP", SIM_MOVE(FuzzyWurmCrawler, AcidGoop), SingleAttackIntent(AcidGoopDamage()));
        MoveState moveState2 = b.Move("ACID_GOOP", SIM_MOVE(FuzzyWurmCrawler, AcidGoop), SingleAttackIntent(AcidGoopDamage()));
        MoveState moveState3 = b.Move("INHALE", SIM_MOVE(FuzzyWurmCrawler, Inhale), BuffIntent());
        moveState.SetFollowUpState(moveState3);
        moveState3.SetFollowUpState(moveState2);
        moveState2.SetFollowUpState(moveState);
        b.Add(moveState);
        b.Add(moveState2);
        b.Add(moveState3);
        return b.Build(moveState);
    }

    void AcidGoop(const CreatureList& targets) const {
        SetIsPuffed(false);
        DamageCmd::Attack(AcidGoopDamage()).FromMonster(*this).Execute(nullptr);
    }
    void Inhale(const CreatureList& targets) const {
        SetIsPuffed(true);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(7), Creature(), CardModel{});
    }
};

}  // namespace sts2
