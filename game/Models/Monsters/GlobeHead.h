#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct GlobeHead : MonsterModel {
    SIM_MODEL(GlobeHead, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t ThunderStrikeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }
    int32_t ShockingSlapDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 14, 13); }
    int32_t GalvanicBurstDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 17, 16); }
    int32_t GalvanicPowerAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 6); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::GALVANIC_POWER, ctx, Creature(), Dec(GalvanicPowerAmount()), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState thunder = b.Move("THUNDER_STRIKE", SIM_MOVE(GlobeHead, ThunderStrike), MultiAttackIntent(ThunderStrikeDamage(), 3));
        MoveState slap = b.Move("SHOCKING_SLAP", SIM_MOVE(GlobeHead, ShockingSlap), SingleAttackIntent(ShockingSlapDamage()), DebuffIntent());
        MoveState burst = b.Move("GALVANIC_BURST", SIM_MOVE(GlobeHead, GalvanicBurstMove), SingleAttackIntent(GalvanicBurstDamage()), BuffIntent());
        slap.SetFollowUpState(thunder);
        thunder.SetFollowUpState(burst);
        burst.SetFollowUpState(slap);
        b.Add(slap);
        b.Add(thunder);
        b.Add(burst);
        return b.Build(slap);
    }

    void ThunderStrike(const CreatureList&) const { DamageCmd::Attack(ThunderStrikeDamage()).WithHitCount(3).FromMonster(*this).Execute(nullptr); }
    void ShockingSlap(const CreatureList& targets) const {
        DamageCmd::Attack(ShockingSlapDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }
    void GalvanicBurstMove(const CreatureList&) const {
        DamageCmd::Attack(GalvanicBurstDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
