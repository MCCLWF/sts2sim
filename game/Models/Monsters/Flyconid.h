// Models/Monsters/Flyconid.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Flyconid : MonsterModel {
    SIM_MODEL(Flyconid, MonsterModel)

    int32_t SmashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 12, 11); }
    int32_t SporeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState vulnerable = b.Move("VULNERABLE_SPORES_MOVE", SIM_MOVE(Flyconid, VulnerableSporesMove), DebuffIntent());
        MoveState frail = b.Move("FRAIL_SPORES_MOVE", SIM_MOVE(Flyconid, FrailSporesMove), SingleAttackIntent(SporeDamage()), DebuffIntent());
        MoveState smash = b.Move("SMASH_MOVE", SIM_MOVE(Flyconid, SmashMove), SingleAttackIntent(SmashDamage()));
        RandomBranchState random = b.RandomBranch("RAND");
        RandomBranchState initial = b.RandomBranch("INITIAL");
        vulnerable.SetFollowUpState(random);
        frail.SetFollowUpState(random);
        smash.SetFollowUpState(random);
        random.AddBranch(vulnerable, 3, MoveRepeatType::CannotRepeat);
        random.AddBranch(frail, 2, MoveRepeatType::CannotRepeat);
        random.AddBranch(smash, MoveRepeatType::CannotRepeat);
        initial.AddBranch(frail, 2, MoveRepeatType::CannotRepeat);
        initial.AddBranch(smash, MoveRepeatType::CannotRepeat);
        b.Add(vulnerable);
        b.Add(frail);
        b.Add(smash);
        b.Add(random);
        b.Add(initial);
        return b.Build(initial);
    }

    void VulnerableSporesMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }
    void FrailSporesMove(const CreatureList& targets) const {
        DamageCmd::Attack(SporeDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }
    void SmashMove(const CreatureList& targets) const { DamageCmd::Attack(SmashDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
