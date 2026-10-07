#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct OwlMagistrate : MonsterModel {
    SIM_MODEL(OwlMagistrate, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_DeathSfx);
    SIM_NOT_PORTED(get_HurtSfx);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    bool IsFlying() const { return extra(0) != 0; }
    void SetIsFlying(bool value) const { extra(0) = value; }

    void BeforeRemovedFromRoom() const {}  // Stops a visual audio loop only.

    int32_t VerdictDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 36, 33); }
    int32_t ScrutinyDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 17, 16); }
    int32_t PeckAssaultDamage() const { return 4; }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState scrutiny = b.Move("MAGISTRATE_SCRUTINY", SIM_MOVE(OwlMagistrate, MagistrateScrutinyMove), SingleAttackIntent(ScrutinyDamage()));
        MoveState peck = b.Move("PECK_ASSAULT", SIM_MOVE(OwlMagistrate, PeckAssaultMove), MultiAttackIntent(PeckAssaultDamage(), 6));
        MoveState flight = b.Move("JUDICIAL_FLIGHT", SIM_MOVE(OwlMagistrate, JudicialFlightMove), BuffIntent());
        MoveState verdict = b.Move("VERDICT", SIM_MOVE(OwlMagistrate, VerdictMove), SingleAttackIntent(VerdictDamage()), DebuffIntent());
        scrutiny.SetFollowUpState(peck);
        peck.SetFollowUpState(flight);
        flight.SetFollowUpState(verdict);
        verdict.SetFollowUpState(scrutiny);
        b.Add(scrutiny);
        b.Add(peck);
        b.Add(flight);
        b.Add(verdict);
        return b.Build(scrutiny);
    }

    void MagistrateScrutinyMove(const CreatureList&) const { DamageCmd::Attack(ScrutinyDamage()).FromMonster(*this).Execute(nullptr); }
    void PeckAssaultMove(const CreatureList&) const { DamageCmd::Attack(PeckAssaultDamage()).WithHitCount(6).FromMonster(*this).Execute(nullptr); }
    void JudicialFlightMove(const CreatureList&) const {
        SetIsFlying(true);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::SOAR_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }
    void VerdictMove(const CreatureList& targets) const {
        SetIsFlying(false);
        DamageCmd::Attack(VerdictDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, targets, Dec(4), Creature(), CardModel{});
        PowerCmd::Remove(PowerId::SOAR_POWER, Creature());
    }
};

}  // namespace sts2
