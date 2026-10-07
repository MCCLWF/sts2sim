// Models/Monsters/Exoskeleton.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Exoskeleton : MonsterModel {
    SIM_MODEL(Exoskeleton, MonsterModel)

    int32_t SkitterRepeats() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }
    int32_t MandiblesDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState skitter = b.Move("SKITTER_MOVE", SIM_MOVE(Exoskeleton, SkitterMove), MultiAttackIntent(1, SkitterRepeats()));
        MoveState mandibles = b.Move("MANDIBLES_MOVE", SIM_MOVE(Exoskeleton, MandiblesMove), SingleAttackIntent(MandiblesDamage()));
        MoveState enrage = b.Move("ENRAGE_MOVE", SIM_MOVE(Exoskeleton, EnrageMove), BuffIntent());
        RandomBranchState random = b.RandomBranch("RAND");
        random.AddBranch(skitter, MoveRepeatType::CannotRepeat, 1.0f);
        random.AddBranch(mandibles, MoveRepeatType::CannotRepeat, 1.0f);
        ConditionalBranchState initial = b.ConditionalBranch("INIT_MOVE");
        initial.AddState(skitter, SIM_COND(Exoskeleton, self.Creature().SlotName() == "first"));
        initial.AddState(mandibles, SIM_COND(Exoskeleton, self.Creature().SlotName() == "second"));
        initial.AddState(enrage, SIM_COND(Exoskeleton, self.Creature().SlotName() == "third"));
        initial.AddState(random, SIM_COND(Exoskeleton, self.Creature().SlotName() == "fourth"));
        skitter.SetFollowUpState(random);
        mandibles.SetFollowUpState(enrage);
        enrage.SetFollowUpState(random);
        b.Add(initial);
        b.Add(random);
        b.Add(skitter);
        b.Add(mandibles);
        b.Add(enrage);
        return b.Build(initial);
    }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::HARD_TO_KILL_POWER, ctx, Creature(), Dec(9), Creature(), CardModel{});
    }

    void SkitterMove(const CreatureList&) const {
        DamageCmd::Attack(1).WithHitCount(SkitterRepeats()).FromMonster(*this).Execute(nullptr);
    }
    void MandiblesMove(const CreatureList&) const { DamageCmd::Attack(MandiblesDamage()).FromMonster(*this).Execute(nullptr); }
    void EnrageMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
