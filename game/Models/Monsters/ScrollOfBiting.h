// Models/Monsters/ScrollOfBiting.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct ScrollOfBiting : MonsterModel {
    SIM_MODEL(ScrollOfBiting, MonsterModel)

    int32_t StarterMoveIdx() const { return extra(0); }
    void SetStarterMoveIdx(int32_t value) const { extra(0) = value; }
    int32_t ChompDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }
    int32_t ChewDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 5); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::PAPER_CUTS_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState chomp = b.Move("CHOMP", SIM_MOVE(ScrollOfBiting, ChompMove), SingleAttackIntent(ChompDamage()));
        MoveState chew = b.Move("CHEW", SIM_MOVE(ScrollOfBiting, ChewMove), MultiAttackIntent(ChewDamage(), 2));
        MoveState teeth = b.Move("MORE_TEETH", SIM_MOVE(ScrollOfBiting, MoreTeethMove), BuffIntent());
        RandomBranchState random = b.RandomBranch("rand");
        chomp.SetFollowUpState(teeth);
        chew.SetFollowUpState(random);
        teeth.SetFollowUpState(chew);
        random.AddBranch(chomp, MoveRepeatType::CannotRepeat);
        random.AddBranch(chew, 2);
        b.Add(chomp);
        b.Add(chew);
        b.Add(teeth);
        b.Add(random);
        return b.Build(StarterMoveIdx() % 3 == 0 ? chomp : StarterMoveIdx() % 3 == 1 ? chew : teeth);
    }

    void ChompMove(const CreatureList&) const { DamageCmd::Attack(ChompDamage()).FromMonster(*this).Execute(nullptr); }
    void ChewMove(const CreatureList&) const { DamageCmd::Attack(ChewDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr); }
    void MoreTeethMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
