// Models/Monsters/TestSubject.cs
#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
namespace sts2 {
struct TestSubject : MonsterModel {
    SIM_MODEL(TestSubject, MonsterModel)
    int32_t Respawns() const { return extra(0); }
    int32_t ExtraMultiClawCount() const { return extra(1); }
    int32_t MultiClawTotalCount() const { return 3 + ExtraMultiClawCount(); }
    bool ShouldDisappearFromDoom() const { return Respawns() >= 2; }
    int32_t FirstFormHp() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 111, 100); }
    int32_t SecondFormHp() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 212, 200); }
    int32_t ThirdFormHp() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::ToughEnemies, 313, 300); }
    int32_t EnrageAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }
    int32_t BiteDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 22, 20); }
    int32_t SkullBashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 16, 14); }
    int32_t MultiClawDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 10); }
    int32_t Phase3LacerateDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 10); }
    int32_t BurningGrowlBurnCount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 3); }
    int32_t BurningGrowlStrengthGain() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }
    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::ADAPTABLE_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::ENRAGE_POWER, ctx, Creature(), Dec(EnrageAmount()), Creature(), CardModel{});
    }
    void AfterDeath(PlayerChoiceContext&, sts2::Creature, bool, float) const {} // Visual color/music only.
    void BeforeRemovedFromRoom() const {} // Visual power event subscriptions.
    void RespawnMove(const CreatureList&) const {
        ++extra(0);
        if (auto power = Creature().GetPower(PowerId::ADAPTABLE_POWER)) power.st().extra[0] = 0;
        int32_t hp = Respawns() == 1 ? SecondFormHp() : ThirdFormHp();
        CreatureCmd::SetMaxHp(Creature(), Dec(hp));
        CreatureCmd::Heal(Creature(), Dec(hp));
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        if (Respawns() == 1) {
            PowerCmd::Apply(PowerId::PAINFUL_STABS_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
        } else if (Respawns() == 2) {
            PowerCmd::Apply(PowerId::NEMESIS_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
            PowerCmd::Remove(Creature().GetPower(PowerId::ADAPTABLE_POWER));
            PowerCmd::Remove(Creature().GetPower(PowerId::PAINFUL_STABS_POWER));
        }
    }
    void BiteMove(const CreatureList&) const { DamageCmd::Attack(BiteDamage()).FromMonster(*this).Execute(nullptr); }
    void SkullBashMove(const CreatureList& targets) const {
        DamageCmd::Attack(SkullBashDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }
    void MultiClawMove(const CreatureList&) const {
        DamageCmd::Attack(MultiClawDamage()).WithHitCount(MultiClawTotalCount()).FromMonster(*this).Execute(nullptr);
        ++extra(1);
    }
    void Phase3LacerateMove(const CreatureList&) const { DamageCmd::Attack(Phase3LacerateDamage()).WithHitCount(3).FromMonster(*this).Execute(nullptr); }
    void BigPounceMove(const CreatureList&) const { DamageCmd::Attack(45).FromMonster(*this).Execute(nullptr); }
    void BurningGrowlMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::BURN, targets, PileType::Discard, BurningGrowlBurnCount(), Player{});
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(BurningGrowlStrengthGain()), Creature(), CardModel{});
    }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        auto respawn = b.Move("RESPAWN_MOVE", SIM_MOVE(TestSubject, RespawnMove), HealIntent(), BuffIntent());
        respawn.SetMustPerformOnceBeforeTransitioning(true);
        auto bite = b.Move("BITE_MOVE", SIM_MOVE(TestSubject, BiteMove), SingleAttackIntent(BiteDamage()));
        auto bash = b.Move("SKULL_BASH_MOVE", SIM_MOVE(TestSubject, SkullBashMove), SingleAttackIntent(SkullBashDamage()), DebuffIntent());
        auto claw = b.Move("MULTI_CLAW_MOVE", SIM_MOVE(TestSubject, MultiClawMove), MultiAttackIntent(MultiClawDamage(), SIM_INTENT_REPEAT(TestSubject, self.MultiClawTotalCount())));
        auto lacerate = b.Move("PHASE3_LACERATE_MOVE", SIM_MOVE(TestSubject, Phase3LacerateMove), MultiAttackIntent(Phase3LacerateDamage(), 3));
        auto pounce = b.Move("BIG_POUNCE", SIM_MOVE(TestSubject, BigPounceMove), SingleAttackIntent(45));
        auto growl = b.Move("BURNING_GROWL_MOVE", SIM_MOVE(TestSubject, BurningGrowlMove), StatusIntent(BurningGrowlBurnCount()), BuffIntent());
        auto branch = b.ConditionalBranch("REVIVE_BRANCH");
        bite.SetFollowUpState(bash); bash.SetFollowUpState(bite); claw.SetFollowUpState(claw);
        lacerate.SetFollowUpState(pounce); pounce.SetFollowUpState(growl); growl.SetFollowUpState(lacerate);
        respawn.SetFollowUpState(branch);
        branch.AddState(claw, SIM_COND(TestSubject, self.Respawns() < 2));
        branch.AddState(lacerate, SIM_COND(TestSubject, self.Respawns() >= 2));
        b.Add(respawn); b.Add(bite); b.Add(bash); b.Add(claw); b.Add(lacerate); b.Add(pounce); b.Add(growl); b.Add(branch);
        return b.Build(bite);
    }
};
}  // namespace sts2
