// Models/Monsters/FossilStalker.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {
struct FossilStalker : MonsterModel {
    SIM_MODEL(FossilStalker, MonsterModel)
    int32_t TackleDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 9); }
    int32_t LatchDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 14, 12); }
    int32_t LashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState tackle = b.Move("TACKLE_MOVE", SIM_MOVE(FossilStalker, TackleMove), SingleAttackIntent(TackleDamage()), DebuffIntent());
        MoveState latch = b.Move("LATCH_MOVE", SIM_MOVE(FossilStalker, LatchMove), SingleAttackIntent(LatchDamage()));
        MoveState lash = b.Move("LASH_MOVE", SIM_MOVE(FossilStalker, LashMove), MultiAttackIntent(LashDamage(), 2));
        RandomBranchState rnd = b.RandomBranch("RAND");
        latch.SetFollowUpState(rnd); tackle.SetFollowUpState(rnd); lash.SetFollowUpState(rnd);
        rnd.AddBranch(latch, 2); rnd.AddBranch(tackle, 2); rnd.AddBranch(lash, 2);
        b.Add(rnd); b.Add(tackle); b.Add(latch); b.Add(lash);
        return b.Build(latch);
    }
    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::SUCK_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }
    void TackleMove(const CreatureList& targets) const {
        DamageCmd::Attack(TackleDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
    void LatchMove(const CreatureList&) const { DamageCmd::Attack(LatchDamage()).FromMonster(*this).Execute(nullptr); }
    void LashMove(const CreatureList&) const { DamageCmd::Attack(LashDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr); }
};
}
