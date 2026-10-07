#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct FlailKnight : MonsterModel {
    SIM_MODEL(FlailKnight, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t FlailDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 10, 9); }
    int32_t RamDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 17, 15); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState chant = b.Move("WAR_CHANT", SIM_MOVE(FlailKnight, WarChantMove), BuffIntent());
        MoveState flail = b.Move("FLAIL_MOVE", SIM_MOVE(FlailKnight, FlailMove), MultiAttackIntent(FlailDamage(), 2));
        MoveState ram = b.Move("RAM_MOVE", SIM_MOVE(FlailKnight, RamMove), SingleAttackIntent(RamDamage()));
        RandomBranchState random = b.RandomBranch("RAND");
        chant.SetFollowUpState(random);
        flail.SetFollowUpState(random);
        ram.SetFollowUpState(random);
        random.AddBranch(chant, MoveRepeatType::CannotRepeat);
        random.AddBranch(flail, 2);
        random.AddBranch(ram, 2);
        b.Add(chant);
        b.Add(flail);
        b.Add(ram);
        b.Add(random);
        return b.Build(ram);
    }

    void WarChantMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }
    void FlailMove(const CreatureList&) const {
        DamageCmd::Attack(FlailDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
    }
    void RamMove(const CreatureList&) const { DamageCmd::Attack(RamDamage()).FromMonster(*this).Execute(nullptr); }
};

struct MysteriousKnight : FlailKnight {
    SIM_MODEL(MysteriousKnight, FlailKnight)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    MoveMachineDef GenerateMoveStateMachine() const { return FlailKnight::GenerateMoveStateMachine(); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(6), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::PLATING_POWER, ctx, Creature(), Dec(6), Creature(), CardModel{});
    }
};

}  // namespace sts2
