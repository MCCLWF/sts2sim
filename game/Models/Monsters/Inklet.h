// Models/Monsters/Inklet.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Inklet : MonsterModel {
    SIM_MODEL(Inklet, MonsterModel)

    bool MiddleInklet() const { return extra(0) != 0; }
    void SetMiddleInklet(bool value) const { extra(0) = value; }

    int32_t JabDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }
    int32_t WhirlwindDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }
    int32_t PiercingGazeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 10); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::SLIPPERY_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState jab = b.Move("JAB_MOVE", SIM_MOVE(Inklet, JabMove), SingleAttackIntent(JabDamage()));
        MoveState whirlwind = b.Move("WHIRLWIND_MOVE", SIM_MOVE(Inklet, WhirlwindMove), MultiAttackIntent(WhirlwindDamage(), 3));
        MoveState gaze = b.Move("PIERCING_GAZE_MOVE", SIM_MOVE(Inklet, PiercingGazeMove), SingleAttackIntent(PiercingGazeDamage()));
        RandomBranchState random = b.RandomBranch("RAND");
        random.AddBranch(gaze, MoveRepeatType::CannotRepeat);
        random.AddBranch(whirlwind, MoveRepeatType::CannotRepeat);
        jab.SetFollowUpState(random);
        whirlwind.SetFollowUpState(jab);
        gaze.SetFollowUpState(jab);
        b.Add(jab);
        b.Add(gaze);
        b.Add(whirlwind);
        b.Add(random);
        return b.Build(MiddleInklet() ? whirlwind : jab);
    }

    void JabMove(const CreatureList& targets) const { DamageCmd::Attack(JabDamage()).FromMonster(*this).Execute(nullptr); }
    void WhirlwindMove(const CreatureList& targets) const { DamageCmd::Attack(WhirlwindDamage()).WithHitCount(3).FromMonster(*this).Execute(nullptr); }
    void PiercingGazeMove(const CreatureList& targets) const { DamageCmd::Attack(PiercingGazeDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
