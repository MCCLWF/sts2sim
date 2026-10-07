// Models/Monsters/TheObscura.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct TheObscura : MonsterModel {
    SIM_MODEL(TheObscura, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t PiercingGazeDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 10);
    }
    int32_t HardeningStrikeDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6);
    }
    int32_t HardeningStrikeBlock() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6);
    }

    void IllusionMove(const CreatureList&) const {
        CreatureCmd::Add(MonsterModel::ToMutable(sim_, MonsterId::PARAFRIGHT), CombatState(), CombatSide::Enemy,
                         "illusion");
    }
    void PiercingGazeMove(const CreatureList&) const {
        DamageCmd::Attack(PiercingGazeDamage()).FromMonster(*this).Execute(nullptr);
    }
    void WailMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, CombatState().GetTeammatesOf(Creature()), Dec(3), Creature(),
                        CardModel{});
    }
    void HardeningStrikeMove(const CreatureList&) const {
        DamageCmd::Attack(HardeningStrikeDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(HardeningStrikeBlock()), ValueProp::Move, nullptr);
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState illusion = b.Move("ILLUSION_MOVE", SIM_MOVE(TheObscura, IllusionMove), SummonIntent());
        MoveState gaze = b.Move("PIERCING_GAZE_MOVE", SIM_MOVE(TheObscura, PiercingGazeMove),
                                SingleAttackIntent(PiercingGazeDamage()));
        MoveState wail = b.Move("SAIL_MOVE", SIM_MOVE(TheObscura, WailMove), BuffIntent());
        MoveState hardening = b.Move("HARDENING_STRIKE_MOVE", SIM_MOVE(TheObscura, HardeningStrikeMove),
                                     SingleAttackIntent(HardeningStrikeDamage()), DefendIntent());
        RandomBranchState random = b.RandomBranch("RAND");
        random.AddBranch(gaze, MoveRepeatType::CannotRepeat);
        random.AddBranch(wail, MoveRepeatType::CannotRepeat);
        random.AddBranch(hardening, MoveRepeatType::CannotRepeat);
        illusion.SetFollowUpState(random);
        gaze.SetFollowUpState(random);
        wail.SetFollowUpState(random);
        hardening.SetFollowUpState(random);
        b.Add(illusion);
        b.Add(gaze);
        b.Add(wail);
        b.Add(hardening);
        b.Add(random);
        return b.Build(illusion);
    }
};

}  // namespace sts2
