#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Axebot : MonsterModel {
    SIM_MODEL(Axebot, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_AttackSfx);
    SIM_NOT_PORTED(get_DeathSfx);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t StockAmount() const { return extra(1) ? extra(0) : 2; }
    void SetStockAmount(int32_t value) const { extra(0) = value; extra(1) = 1; }
    int32_t RespawnCount() const { return 2 - StockAmount(); }

    int32_t BootUpBlock() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 15, 10); }
    int32_t OneTwoDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 10); }
    int32_t BootUpStrGain() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }
    int32_t HammerUppercutDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 18, 14); }

    void AfterAddedToRoom() const {
        if (StockAmount() > 0) {
            PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
            PowerCmd::Apply(PowerId::STOCK_POWER, ctx, Creature(), Dec(StockAmount()), sts2::Creature{}, CardModel{});
        }
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState bootUp = b.Move("BOOT_UP_MOVE", SIM_MOVE(Axebot, BootUpMove), DefendIntent(), BuffIntent());
        MoveState oneTwo = b.Move("ONE_TWO_MOVE", SIM_MOVE(Axebot, OneTwoMove), MultiAttackIntent(OneTwoDamage(), 2));
        MoveState uppercut = b.Move("HAMMER_UPPERCUT_MOVE", SIM_MOVE(Axebot, HammerUppercutMove), SingleAttackIntent(HammerUppercutDamage()), DebuffIntent());
        bootUp.SetFollowUpState(uppercut);
        uppercut.SetFollowUpState(oneTwo);
        oneTwo.SetFollowUpState(uppercut);
        b.Add(bootUp);
        b.Add(oneTwo);
        b.Add(uppercut);
        return b.Build(extra(1) ? bootUp : uppercut);
    }

    void BootUpMove(const CreatureList&) const {
        CreatureCmd::GainBlock(Creature(), Dec(BootUpBlock()), ValueProp::Move, nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(BootUpStrGain() * RespawnCount()), Creature(), CardModel{});
    }
    void OneTwoMove(const CreatureList&) const { DamageCmd::Attack(OneTwoDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr); }
    void HammerUppercutMove(const CreatureList& targets) const {
        DamageCmd::Attack(HammerUppercutDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
