#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct CeremonialBeast : MonsterModel {
    SIM_MODEL(CeremonialBeast, MonsterModel)

    bool IsStunnedByPlowRemoval() const { return extra(0) != 0; }
    void SetIsStunnedByPlowRemoval() const { extra(0) = 1; extra(1) = 1; }
    void ClearIsStunnedByPlowRemoval() const { extra(0) = 0; }
    bool IsInSecondPhase() const { return extra(1) != 0; }

    int32_t PlowAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 160, 150); }
    int32_t PlowDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 20, 18); }
    int32_t StompDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 17, 15); }
    int32_t CrushDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 19, 17); }
    int32_t CrushStrength() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }

    bool ShouldDisappearFromDoom() const { return false; }
    bool ShouldFadeAfterDeath() const { return false; }

    void AfterAddedToRoom() const {}

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState stamp = b.Move("STAMP_MOVE", SIM_MOVE(CeremonialBeast, StampMove), BuffIntent());
        MoveState plow = b.Move("PLOW_MOVE", SIM_MOVE(CeremonialBeast, PlowMove),
                                SingleAttackIntent(PlowDamage()), BuffIntent());
        MoveState stunned = b.Move("STUN_MOVE", SIM_MOVE(CeremonialBeast, StunnedMove), StunIntent());
        stunned.SetMustPerformOnceBeforeTransitioning(true);
        MoveState cry = b.Move("BEAST_CRY_MOVE", SIM_MOVE(CeremonialBeast, BeastCryMove), DebuffIntent());
        MoveState stomp = b.Move("STOMP_MOVE", SIM_MOVE(CeremonialBeast, StompMove), SingleAttackIntent(StompDamage()));
        MoveState crush = b.Move("CRUSH_MOVE", SIM_MOVE(CeremonialBeast, CrushMove),
                                 SingleAttackIntent(CrushDamage()), BuffIntent());
        stunned.SetFollowUpState(cry);
        cry.SetFollowUpState(stomp);
        stomp.SetFollowUpState(crush);
        crush.SetFollowUpState(cry);
        stamp.SetFollowUpState(plow);
        plow.SetFollowUpState(plow);
        b.Add(stamp);
        b.Add(plow);
        b.Add(stunned);
        b.Add(cry);
        b.Add(stomp);
        b.Add(crush);
        return b.Build(stamp);
    }

    void StampMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::PLOW_POWER, ctx, Creature(), Dec(PlowAmount()), Creature(), CardModel{});
    }
    void PlowMove(const CreatureList&) const {
        DamageCmd::Attack(PlowDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
    void StunnedMove(const CreatureList&) const { ClearIsStunnedByPlowRemoval(); }
    void BeastCryMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::RINGING_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
    void StompMove(const CreatureList&) const { DamageCmd::Attack(StompDamage()).FromMonster(*this).Execute(nullptr); }
    void CrushMove(const CreatureList&) const {
        DamageCmd::Attack(CrushDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(CrushStrength()), Creature(), CardModel{});
    }
};

}  // namespace sts2
