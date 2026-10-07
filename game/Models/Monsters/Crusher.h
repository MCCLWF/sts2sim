// Models/Monsters/Crusher.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
namespace sts2 {
struct Crusher : MonsterModel {
    SIM_MODEL(Crusher, MonsterModel)
    bool ShouldFadeAfterDeath() const { return false; }
    bool ShouldDisappearFromDoom() const { return false; }
    int32_t ThrashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 14, 12); }
    int32_t EnlargingStrikeDamage() const { return 4; }
    int32_t BugStingDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }
    int32_t AdaptStrengthGain() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }
    int32_t GuardedStrikeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 14, 12); }
    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::BACK_ATTACK_LEFT_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::CRAB_RAGE_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }
    void AfterCurrentHpChanged(sts2::Creature, Dec) const {} // Visual background damage animation.
    void BeforeDeath(sts2::Creature) const {} // Audio and background animation.
    void ThrashMove(const CreatureList&) const { DamageCmd::Attack(ThrashDamage()).FromMonster(*this).Execute(nullptr); }
    void EnlargingStrikeMove(const CreatureList&) const { DamageCmd::Attack(EnlargingStrikeDamage()).FromMonster(*this).Execute(nullptr); }
    void BugStingMove(const CreatureList& targets) const {
        DamageCmd::Attack(BugStingDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }
    void AdaptMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(AdaptStrengthGain()), Creature(), CardModel{});
    }
    void GuardedStrikeMove(const CreatureList&) const {
        DamageCmd::Attack(GuardedStrikeDamage()).FromMonster(*this).Execute(nullptr);
        CreatureCmd::GainBlock(Creature(), Dec(18), ValueProp::Move, nullptr);
    }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        auto thrash = b.Move("THRASH_MOVE", SIM_MOVE(Crusher, ThrashMove), SingleAttackIntent(ThrashDamage()));
        auto enlarging = b.Move("ENLARGING_STRIKE_MOVE", SIM_MOVE(Crusher, EnlargingStrikeMove), SingleAttackIntent(EnlargingStrikeDamage()));
        auto sting = b.Move("BUG_STING_MOVE", SIM_MOVE(Crusher, BugStingMove), MultiAttackIntent(BugStingDamage(), 2), DebuffIntent());
        auto adapt = b.Move("ADAPT_MOVE", SIM_MOVE(Crusher, AdaptMove), BuffIntent());
        auto guarded = b.Move("GUARDED_STRIKE_MOVE", SIM_MOVE(Crusher, GuardedStrikeMove), SingleAttackIntent(GuardedStrikeDamage()), DefendIntent());
        thrash.SetFollowUpState(enlarging); enlarging.SetFollowUpState(sting); sting.SetFollowUpState(adapt); adapt.SetFollowUpState(guarded); guarded.SetFollowUpState(thrash);
        b.Add(thrash); b.Add(enlarging); b.Add(sting); b.Add(adapt); b.Add(guarded);
        return b.Build(thrash);
    }
};
}  // namespace sts2
