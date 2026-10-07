// Models/Monsters/InfestedPrism.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct InfestedPrism : MonsterModel {
    SIM_MODEL(InfestedPrism, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_AttackSfx);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t JabDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 17, 15); }
    int32_t VitalSparkAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }
    int32_t PulsateDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 10, 8); }
    int32_t PulsateBlock() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 22, 20); }
    int32_t RadiateDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 13, 11); }
    int32_t RadiateBlock() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 13, 11); }
    int32_t WhirlwindDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 5); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VITAL_SPARK_POWER, ctx, Creature(), Dec(VitalSparkAmount()), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState jab = b.Move("JAB_MOVE", SIM_MOVE(InfestedPrism, JabMove), SingleAttackIntent(JabDamage()));
        MoveState radiate = b.Move("RADIATE_MOVE", SIM_MOVE(InfestedPrism, RadiateMove), SingleAttackIntent(RadiateDamage()), DefendIntent());
        MoveState whirlwind = b.Move("WHIRLWIND_MOVE", SIM_MOVE(InfestedPrism, WhirlwindMove), MultiAttackIntent(WhirlwindDamage(), 3));
        MoveState pulsate = b.Move("PULSATE_MOVE", SIM_MOVE(InfestedPrism, PulsateMove), SingleAttackIntent(PulsateDamage()), BuffIntent(), DefendIntent());
        jab.SetFollowUpState(radiate);
        radiate.SetFollowUpState(whirlwind);
        whirlwind.SetFollowUpState(pulsate);
        pulsate.SetFollowUpState(jab);
        b.Add(jab);
        b.Add(radiate);
        b.Add(whirlwind);
        b.Add(pulsate);
        return b.Build(jab);
    }

    void JabMove(const CreatureList&) const { DamageCmd::Attack(JabDamage()).FromMonster(*this).Execute(nullptr); }
    void RadiateMove(const CreatureList&) const {
        DamageCmd::Attack(RadiateDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(RadiateBlock()), ValueProp::Move, nullptr);
    }
    void WhirlwindMove(const CreatureList&) const {
        DamageCmd::Attack(WhirlwindDamage()).WithHitCount(3).FromMonster(*this).Execute(nullptr);
    }
    void PulsateMove(const CreatureList&) const {
        DamageCmd::Attack(PulsateDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(PulsateBlock()), ValueProp::Move, nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VITAL_SPARK_POWER, ctx, Creature(), Dec(VitalSparkAmount()), Creature(), CardModel{});
    }
};

}  // namespace sts2
