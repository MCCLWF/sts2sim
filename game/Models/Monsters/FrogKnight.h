// Models/Monsters/FrogKnight.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct FrogKnight : MonsterModel {
    SIM_MODEL(FrogKnight, MonsterModel)

    bool HasBeetleCharged() const { return extra(0) != 0; }
    void SetHasBeetleCharged(bool value) const { extra(0) = value; }
    int32_t StrikeDownEvilDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 23, 21); }
    int32_t TongueLashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 14, 13); }
    int32_t BeetleChargeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 40, 35); }
    int32_t PlatingAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 19, 15); }

    void AfterAddedToRoom() const {
        SetHasBeetleCharged(false);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::PLATING_POWER, ctx, Creature(), Dec(PlatingAmount()), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState queen = b.Move("FOR_THE_QUEEN", SIM_MOVE(FrogKnight, ForTheQueenMove), BuffIntent());
        MoveState strike = b.Move("STRIKE_DOWN_EVIL", SIM_MOVE(FrogKnight, StrikeDownEvilMove), SingleAttackIntent(StrikeDownEvilDamage()));
        MoveState lash = b.Move("TONGUE_LASH", SIM_MOVE(FrogKnight, TongueLashMove), SingleAttackIntent(TongueLashDamage()), DebuffIntent());
        MoveState charge = b.Move("BEETLE_CHARGE", SIM_MOVE(FrogKnight, BeetleChargeMove), SingleAttackIntent(BeetleChargeDamage()));
        ConditionalBranchState half = b.ConditionalBranch("HALF_HEALTH");
        half.AddState(lash, SIM_COND(FrogKnight, self.HasBeetleCharged() || self.Creature().CurrentHp() >= self.Creature().MaxHp() / 2));
        half.AddState(charge, SIM_COND(FrogKnight, !self.HasBeetleCharged() && self.Creature().CurrentHp() < self.Creature().MaxHp() / 2));
        queen.SetFollowUpState(half);
        strike.SetFollowUpState(queen);
        lash.SetFollowUpState(strike);
        charge.SetFollowUpState(lash);
        b.Add(half);
        b.Add(queen);
        b.Add(strike);
        b.Add(lash);
        b.Add(charge);
        return b.Build(lash);
    }

    void ForTheQueenMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(5), Creature(), CardModel{});
    }

    void StrikeDownEvilMove(const CreatureList&) const {
        DamageCmd::Attack(StrikeDownEvilDamage()).FromMonster(*this).Execute(nullptr);
    }

    void TongueLashMove(const CreatureList& targets) const {
        DamageCmd::Attack(TongueLashDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }

    void BeetleChargeMove(const CreatureList&) const {
        SetHasBeetleCharged(true);
        DamageCmd::Attack(BeetleChargeDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
