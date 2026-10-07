// Models/Monsters/Nibbit.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Nibbit : MonsterModel {
    SIM_MODEL(Nibbit, MonsterModel)

    bool IsFront() const { return extra(0) != 0; }  // _isFront
    void SetIsFront(bool value) const { extra(0) = value; }
    bool IsAlone() const { return extra(1) != 0; }  // _isAlone
    void SetIsAlone(bool value) const { extra(1) = value; }

    int32_t ButtDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 13, 12); }
    int32_t SliceBlock() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 6, 5); }
    int32_t SliceDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }
    int32_t HissStrengthGain() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("BUTT_MOVE", SIM_MOVE(Nibbit, ButtMove), SingleAttackIntent(ButtDamage()));
        MoveState moveState2 = b.Move("SLICE_MOVE", SIM_MOVE(Nibbit, SliceMove), SingleAttackIntent(SliceDamage()), DefendIntent());
        MoveState moveState3 = b.Move("HISS_MOVE", SIM_MOVE(Nibbit, HissMove), BuffIntent());
        ConditionalBranchState conditionalBranchState = b.ConditionalBranch("INIT_MOVE");
        if (IsAlone()) {
            conditionalBranchState.AddState(moveState, SIM_COND(Nibbit, Nibbit(self.Creature().Monster()).IsAlone()));
        } else {
            conditionalBranchState.AddState(moveState3, SIM_COND(Nibbit, !Nibbit(self.Creature().Monster()).IsFront()));
            conditionalBranchState.AddState(moveState2, SIM_COND(Nibbit, Nibbit(self.Creature().Monster()).IsFront()));
        }
        moveState2.SetFollowUpState(moveState3);
        moveState.SetFollowUpState(moveState2);
        moveState3.SetFollowUpState(moveState);
        b.Add(conditionalBranchState);
        b.Add(moveState);
        b.Add(moveState2);
        b.Add(moveState3);
        return b.Build(conditionalBranchState);
    }

    void ButtMove(const CreatureList& targets) const { DamageCmd::Attack(ButtDamage()).FromMonster(*this).Execute(nullptr); }
    void SliceMove(const CreatureList& targets) const {
        DamageCmd::Attack(SliceDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(SliceBlock()), ValueProp::Move, nullptr);
    }
    void HissMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(HissStrengthGain()), Creature(), CardModel{});
    }
};

}  // namespace sts2
