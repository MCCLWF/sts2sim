// Models/Monsters/MechaKnight.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct MechaKnight : MonsterModel {
    SIM_MODEL(MechaKnight, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t ChargeDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 30, 25);
    }
    int32_t FlamethrowerDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 12, 8);
    }
    int32_t HeavyCleaveDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 40, 35);
    }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::ARTIFACT_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState charge = b.Move("CHARGE_MOVE", SIM_MOVE(MechaKnight, ChargeMove), SingleAttackIntent(ChargeDamage()));
        MoveState flamethrower = b.Move("FLAMETHROWER_MOVE", SIM_MOVE(MechaKnight, FlamethrowerMove),
                                        SingleAttackIntent(FlamethrowerDamage()), StatusIntent(4));
        MoveState windup = b.Move("WINDUP_MOVE", SIM_MOVE(MechaKnight, WindupMove), DefendIntent(), BuffIntent());
        MoveState cleave = b.Move("HEAVY_CLEAVE_MOVE", SIM_MOVE(MechaKnight, HeavyCleaveMove),
                                  SingleAttackIntent(HeavyCleaveDamage()));
        charge.SetFollowUpState(flamethrower);
        flamethrower.SetFollowUpState(windup);
        windup.SetFollowUpState(cleave);
        cleave.SetFollowUpState(flamethrower);
        b.Add(charge);
        b.Add(flamethrower);
        b.Add(windup);
        b.Add(cleave);
        return b.Build(charge);
    }

    void ChargeMove(const CreatureList&) const {
        DamageCmd::Attack(ChargeDamage()).FromMonster(*this).Execute(nullptr);
    }
    void FlamethrowerMove(const CreatureList& targets) const {
        DamageCmd::Attack(FlamethrowerDamage()).FromMonster(*this).Execute(nullptr);
        CardPileCmd::AddToCombatAndPreview(CardId::BURN, targets, PileType::Hand, 4, Player{});
    }
    void WindupMove(const CreatureList&) const {
        CreatureCmd::GainBlock(Creature(), Dec(15), ValueProp::Move, nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(5), Creature(), CardModel{});
    }
    void HeavyCleaveMove(const CreatureList&) const {
        DamageCmd::Attack(HeavyCleaveDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
