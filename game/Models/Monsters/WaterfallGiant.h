// Models/Monsters/WaterfallGiant.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct WaterfallGiant : MonsterModel {
    SIM_MODEL(WaterfallGiant, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t PressureGunDamage() const { return extra(0) == 0 ? BasePressureGunDamage() : extra(0); }
    int32_t SteamEruptionDamage() const { return extra(1); }
    int32_t PressureBuildupIdx() const { return extra(2); }
    int32_t SiphonHeal() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 15, 10); }
    int32_t PressurizeAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 20, 15); }
    int32_t StompDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 15); }
    int32_t RamDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 10); }
    int32_t PressureUpDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 14, 13); }
    int32_t BasePressureGunDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 23, 20); }

    bool ShouldDisappearFromDoom() const { return !Creature().HasPower(PowerId::STEAM_ERUPTION_POWER); }
    bool ShouldFadeAfterDeath() const { return PressureBuildupIdx() == 0; }

    void AfterAddedToRoom() const { extra(0) = BasePressureGunDamage(); }
    // Source hooks only adjust music and looping audio here.
    void AfterDeath(PlayerChoiceContext&, sts2::Creature, bool, float) const {}
    void BeforeRemovedFromRoom() const {}

    void TriggerAboutToBlowState() {
        CreatureCmd::SetMaxAndCurrentHp(Creature(), Dec(999999999));
        Creature().SetHpDisplay(HpDisplay::InfiniteWithoutNumbers);
        SetMoveImmediate("ABOUT_TO_BLOW_MOVE", true);
    }

    void PressurizeMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STEAM_ERUPTION_POWER, ctx, Creature(), Dec(PressurizeAmount()), Creature(), CardModel{});
        ++extra(2);
    }
    void StompMove(const CreatureList& targets) const {
        DamageCmd::Attack(StompDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::STEAM_ERUPTION_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
        ++extra(2);
    }
    void RamMove(const CreatureList&) const {
        DamageCmd::Attack(RamDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STEAM_ERUPTION_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
        ++extra(2);
    }
    void SiphonMove(const CreatureList&) const {
        CreatureCmd::Heal(Creature(), Dec(SiphonHeal() * static_cast<int32_t>(CombatState().Players().size())));
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STEAM_ERUPTION_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
        ++extra(2);
    }
    void PressureGunMove(const CreatureList&) const {
        DamageCmd::Attack(PressureGunDamage()).FromMonster(*this).Execute(nullptr);
        extra(0) = PressureGunDamage() + 5;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STEAM_ERUPTION_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
        ++extra(2);
    }
    void PressureUpMove(const CreatureList&) const {
        DamageCmd::Attack(PressureUpDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STEAM_ERUPTION_POWER, ctx, Creature(), Dec(3), Creature(), CardModel{});
        ++extra(2);
    }
    void AboutToBlowMove(const CreatureList&) const {
        extra(1) = Creature().GetPowerAmount(PowerId::STEAM_ERUPTION_POWER);
        PowerCmd::Remove(PowerId::STEAM_ERUPTION_POWER, Creature());
        extra(2) = 6;
        ++extra(2);
    }
    void ExplodeMove(const CreatureList&) const {
        DamageCmd::Attack(SteamEruptionDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::Kill(Creature());
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState pressurize = b.Move("PRESSURIZE_MOVE", SIM_MOVE(WaterfallGiant, PressurizeMove), BuffIntent());
        MoveState stomp = b.Move("STOMP_MOVE", SIM_MOVE(WaterfallGiant, StompMove),
                                 SingleAttackIntent(StompDamage()), DebuffIntent(), BuffIntent());
        MoveState ram = b.Move("RAM_MOVE", SIM_MOVE(WaterfallGiant, RamMove), SingleAttackIntent(RamDamage()), BuffIntent());
        MoveState siphon = b.Move("SIPHON_MOVE", SIM_MOVE(WaterfallGiant, SiphonMove), HealIntent(), BuffIntent());
        MoveState pressureGun = b.Move("PRESSURE_GUN_MOVE", SIM_MOVE(WaterfallGiant, PressureGunMove),
                                       SingleAttackIntent(SIM_INTENT_DAMAGE(WaterfallGiant, self.PressureGunDamage())),
                                       BuffIntent());
        MoveState pressureUp = b.Move("PRESSURE_UP_MOVE", SIM_MOVE(WaterfallGiant, PressureUpMove),
                                      SingleAttackIntent(PressureUpDamage()), BuffIntent());
        MoveState aboutToBlow = b.Move("ABOUT_TO_BLOW_MOVE", SIM_MOVE(WaterfallGiant, AboutToBlowMove), StunIntent());
        aboutToBlow.SetMustPerformOnceBeforeTransitioning(true);
        MoveState explode = b.Move("EXPLODE_MOVE", SIM_MOVE(WaterfallGiant, ExplodeMove),
                                   DeathBlowIntent(SIM_INTENT_DAMAGE(WaterfallGiant, self.SteamEruptionDamage())));
        pressurize.SetFollowUpState(stomp);
        stomp.SetFollowUpState(ram);
        ram.SetFollowUpState(siphon);
        siphon.SetFollowUpState(pressureGun);
        pressureGun.SetFollowUpState(pressureUp);
        pressureUp.SetFollowUpState(stomp);
        aboutToBlow.SetFollowUpState(explode);
        explode.SetFollowUpState(explode);
        b.Add(pressurize);
        b.Add(stomp);
        b.Add(ram);
        b.Add(siphon);
        b.Add(pressureGun);
        b.Add(pressureUp);
        b.Add(explode);
        b.Add(aboutToBlow);
        return b.Build(pressurize);
    }
};

}  // namespace sts2
