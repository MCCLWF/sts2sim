// Models/Monsters/CubexConstruct.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct CubexConstruct : MonsterModel {
    SIM_MODEL(CubexConstruct, MonsterModel)

    bool IsBurrowed() const { return extra(0) != 0; }
    void SetIsBurrowed(bool value) const { extra(0) = value; }
    bool IsCharging() const { return extra(1) != 0; }
    void SetIsCharging(bool value) const { extra(1) = value; }

    int32_t BlastDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7); }
    int32_t ExpelDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 5); }

    void AfterAddedToRoom() const {
        CreatureCmd::GainBlock(Creature(), Dec(13), ValueProp::Move, nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::ARTIFACT_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
        SetIsBurrowed(true);
    }
    void BeforeRemovedFromRoom() const {}

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState charge = b.Move("CHARGE_UP_MOVE", SIM_MOVE(CubexConstruct, ChargeUpMove), BuffIntent());
        MoveState blast = b.Move("REPEATER_BLAST_MOVE", SIM_MOVE(CubexConstruct, RepeaterBlastMove), SingleAttackIntent(BlastDamage()), BuffIntent());
        MoveState blast2 = b.Move("REPEATER_BLAST_MOVE_2", SIM_MOVE(CubexConstruct, RepeaterBlastMove), SingleAttackIntent(BlastDamage()), BuffIntent());
        MoveState expel = b.Move("EXPEL_MOVE", SIM_MOVE(CubexConstruct, ExpelBlastMove), MultiAttackIntent(ExpelDamage(), 2));
        charge.SetFollowUpState(blast);
        blast.SetFollowUpState(blast2);
        blast2.SetFollowUpState(expel);
        expel.SetFollowUpState(blast);
        b.Add(charge);
        b.Add(blast);
        b.Add(blast2);
        b.Add(expel);
        return b.Build(charge);
    }

    void ChargeUpMove(const CreatureList& targets) const {
        SetIsBurrowed(false);
        SetIsCharging(true);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
    void RepeaterBlastMove(const CreatureList& targets) const {
        DamageCmd::Attack(BlastDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
    void ExpelBlastMove(const CreatureList& targets) const { DamageCmd::Attack(ExpelDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
