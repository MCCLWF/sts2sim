// Models/Monsters/Toadpole.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Toadpole : MonsterModel {
    SIM_MODEL(Toadpole, MonsterModel)

    bool IsFront() const { return extra(0) != 0; }  // _isFront
    void SetIsFront(bool value) const { extra(0) = value; }

    int32_t SpikeSpitDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }
    int32_t SpikeSpitRepeat() const { return 3; }
    int32_t WhirlDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }
    int32_t SpikenAmount() const { return 2; }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState moveState = b.Move("SPIKE_SPIT_MOVE", SIM_MOVE(Toadpole, SpikeSpitMove), MultiAttackIntent(SpikeSpitDamage(), SpikeSpitRepeat()));
        MoveState moveState2 = b.Move("WHIRL_MOVE", SIM_MOVE(Toadpole, WhirlMove), SingleAttackIntent(WhirlDamage()));
        MoveState moveState3 = b.Move("SPIKEN_MOVE", SIM_MOVE(Toadpole, SpikenMove), BuffIntent());
        ConditionalBranchState conditionalBranchState = b.ConditionalBranch("INIT_MOVE");
        moveState2.SetFollowUpState(moveState3);
        moveState3.SetFollowUpState(moveState);
        moveState.SetFollowUpState(moveState2);
        conditionalBranchState.AddState(moveState2, SIM_COND(Toadpole, !Toadpole(self.Creature().Monster()).IsFront()));
        conditionalBranchState.AddState(moveState3, SIM_COND(Toadpole, Toadpole(self.Creature().Monster()).IsFront()));
        b.Add(conditionalBranchState);
        b.Add(moveState3);
        b.Add(moveState);
        b.Add(moveState2);
        return b.Build(conditionalBranchState);
    }

    void SpikeSpitMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::THORNS_POWER, ctx, Creature(), Dec(-SpikenAmount()), Creature(), CardModel{});
        DamageCmd::Attack(SpikeSpitDamage()).WithHitCount(SpikeSpitRepeat()).FromMonster(*this).Execute(nullptr);
    }
    void WhirlMove(const CreatureList& targets) const { DamageCmd::Attack(WhirlDamage()).FromMonster(*this).Execute(nullptr); }
    void SpikenMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::THORNS_POWER, ctx, Creature(), Dec(SpikenAmount()), Creature(), CardModel{});
    }
};

}  // namespace sts2
