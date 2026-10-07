// Models/Monsters/Rocket.cs
#pragma once
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
namespace sts2 {
struct Rocket : MonsterModel {
    SIM_MODEL(Rocket, MonsterModel)
    bool ShouldFadeAfterDeath() const { return false; }
    bool ShouldDisappearFromDoom() const { return false; }
    int32_t TargetingReticleDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }
    int32_t PrecisionBeamDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 20, 18); }
    int32_t LaserDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 35, 31); }
    int32_t ChargeUpStrengthGain() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }
    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::SURROUNDED_POWER, ctx, CombatState().GetOpponentsOf(Creature()), Dec(1), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::BACK_ATTACK_RIGHT_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::CRAB_RAGE_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }
    void AfterCurrentHpChanged(sts2::Creature, Dec) const {} // Visual background damage animation.
    void BeforeDeath(sts2::Creature) const {} // Audio and background animation.
    void BeforeRemovedFromRoom() const {} // Stops audio loop.
    void TargetingReticleMove(const CreatureList&) const { DamageCmd::Attack(TargetingReticleDamage()).FromMonster(*this).Execute(nullptr); }
    void PrecisionBeamMove(const CreatureList&) const { DamageCmd::Attack(PrecisionBeamDamage()).FromMonster(*this).Execute(nullptr); }
    void ChargeUpMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(ChargeUpStrengthGain()), Creature(), CardModel{});
    }
    void LaserMove(const CreatureList&) const { DamageCmd::Attack(LaserDamage()).FromMonster(*this).Execute(nullptr); }
    void RechargeMove(const CreatureList&) const {} // Presentation-only.
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        auto reticle = b.Move("TARGETING_RETICLE_MOVE", SIM_MOVE(Rocket, TargetingReticleMove), SingleAttackIntent(TargetingReticleDamage()));
        auto beam = b.Move("PRECISION_BEAM_MOVE", SIM_MOVE(Rocket, PrecisionBeamMove), SingleAttackIntent(PrecisionBeamDamage()));
        auto charge = b.Move("CHARGE_UP_MOVE", SIM_MOVE(Rocket, ChargeUpMove), BuffIntent());
        auto laser = b.Move("LASER_MOVE", SIM_MOVE(Rocket, LaserMove), SingleAttackIntent(LaserDamage()));
        auto recharge = b.Move("RECHARGE_MOVE", SIM_MOVE(Rocket, RechargeMove), SleepIntent());
        reticle.SetFollowUpState(beam); beam.SetFollowUpState(charge); charge.SetFollowUpState(laser); laser.SetFollowUpState(recharge); recharge.SetFollowUpState(reticle);
        b.Add(reticle); b.Add(beam); b.Add(charge); b.Add(laser); b.Add(recharge);
        return b.Build(reticle);
    }
};
}  // namespace sts2
