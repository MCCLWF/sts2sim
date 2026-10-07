#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct BowlbugSilk : MonsterModel {
    SIM_MODEL(BowlbugSilk, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(SetupSkins);
    SIM_NOT_PORTED(get_DeathSfx);
    SIM_NOT_PORTED(get_MinInitialHp);
    SIM_NOT_PORTED(get_MaxInitialHp);

    int32_t ThrashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 4); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState thrash = b.Move("THRASH_MOVE", SIM_MOVE(BowlbugSilk, ThrashMove), MultiAttackIntent(ThrashDamage(), 2));
        MoveState spit = b.Move("TOXIC_SPIT_MOVE", SIM_MOVE(BowlbugSilk, SpitMove), DebuffIntent());
        thrash.SetFollowUpState(spit);
        spit.SetFollowUpState(thrash);
        b.Add(thrash);
        b.Add(spit);
        return b.Build(spit);
    }

    void ThrashMove(const CreatureList&) const { DamageCmd::Attack(ThrashDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr); }
    void SpitMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
};

}  // namespace sts2
