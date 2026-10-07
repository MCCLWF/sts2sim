// Models/Monsters/HunterKiller.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct HunterKiller : MonsterModel {
    SIM_MODEL(HunterKiller, MonsterModel)

    int32_t BiteDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 19, 17); }
    int32_t PunctureDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState goop = b.Move("TENDERIZING_GOOP_MOVE", SIM_MOVE(HunterKiller, GoopMove), DebuffIntent());
        MoveState bite = b.Move("BITE_MOVE", SIM_MOVE(HunterKiller, BiteMove), SingleAttackIntent(BiteDamage()));
        MoveState puncture = b.Move("PUNCTURE_MOVE", SIM_MOVE(HunterKiller, PunctureMove), MultiAttackIntent(PunctureDamage(), 3));
        RandomBranchState random = b.RandomBranch("RAND");
        goop.SetFollowUpState(random);
        bite.SetFollowUpState(random);
        puncture.SetFollowUpState(random);
        random.AddBranch(bite, MoveRepeatType::CannotRepeat);
        random.AddBranch(puncture, 2);
        b.Add(goop);
        b.Add(bite);
        b.Add(puncture);
        b.Add(random);
        return b.Build(goop);
    }

    void GoopMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::TENDER_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
    void BiteMove(const CreatureList&) const { DamageCmd::Attack(BiteDamage()).FromMonster(*this).Execute(nullptr); }
    void PunctureMove(const CreatureList&) const {
        DamageCmd::Attack(PunctureDamage()).WithHitCount(3).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
