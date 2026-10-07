// Models/Monsters/Queen.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {
struct Queen : MonsterModel {
    SIM_MODEL(Queen, MonsterModel)

    bool HasAmalgamDied() const { return extra(0) != 0; }
    int32_t OffWithYourHeadDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }
    int32_t ExecutionDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 18, 15); }
    void BeforeRemovedFromRoom() const {}  // Source resets only an animation track.
    void AfterAddedToRoom() const {
        for (auto enemy : CombatState().Enemies()) {
            if (enemy.Monster().MonsterId_() == MonsterId::TORCH_HEAD_AMALGAM) { extra(1) = enemy.Index(); return; }
        }
        throw GameError("Queen requires TorchHeadAmalgam");
    }
    void PuppetStringsMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::CHAINS_OF_BINDING_POWER, ctx, targets, Dec(3), Creature(), CardModel{});
    }
    void YoureMineMove(const CreatureList& targets) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(99), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(99), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, targets, Dec(99), Creature(), CardModel{});
    }
    void BurnBrightForMeMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        for (auto teammate : CombatState().GetTeammatesOf(Creature())) {
            if (teammate != Creature()) PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, teammate, Dec(1), Creature(), CardModel{});
        }
        CreatureCmd::GainBlock(Creature(), Dec(20), ValueProp::Move, nullptr);
    }
    void OffWithYourHeadMove(const CreatureList&) const {
        DamageCmd::Attack(OffWithYourHeadDamage()).WithHitCount(5).FromMonster(*this).OnlyPlayAnimOnce().Execute(nullptr);
    }
    void ExecutionMove(const CreatureList&) const { DamageCmd::Attack(ExecutionDamage()).FromMonster(*this).Execute(nullptr); }
    void EnrageMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }
    void AfterDeath(PlayerChoiceContext&, sts2::Creature creature, bool, float) const {
        auto monster = creature.Monster();
        if (monster && monster.MonsterId_() == MonsterId::TORCH_HEAD_AMALGAM && Creature().IsAlive()) {
            extra(0) = 1;
            extra(1) = 0;
            if (NextMove().Id() == "BURN_BRIGHT_FOR_ME_MOVE") Queen(*this).SetMoveImmediate("ENRAGE_MOVE");
        }
    }
    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        auto puppet = b.Move("PUPPET_STRINGS_MOVE", SIM_MOVE(Queen, PuppetStringsMove), CardDebuffIntent());
        auto mine = b.Move("YOU_ARE_MINE_MOVE", SIM_MOVE(Queen, YoureMineMove), DebuffIntent());
        auto mineBranch = b.ConditionalBranch("YOURE_MINE_NOW_BRANCH");
        auto burn = b.Move("BURN_BRIGHT_FOR_ME_MOVE", SIM_MOVE(Queen, BurnBrightForMeMove), BuffIntent(), DefendIntent());
        auto burnBranch = b.ConditionalBranch("BURN_BRIGHT_FOR_ME_BRANCH");
        auto head = b.Move("OFF_WITH_YOUR_HEAD_MOVE", SIM_MOVE(Queen, OffWithYourHeadMove), MultiAttackIntent(OffWithYourHeadDamage(), 5));
        auto execution = b.Move("EXECUTION_MOVE", SIM_MOVE(Queen, ExecutionMove), SingleAttackIntent(ExecutionDamage()));
        auto enrage = b.Move("ENRAGE_MOVE", SIM_MOVE(Queen, EnrageMove), BuffIntent());
        puppet.SetFollowUpState(mine);
        mine.SetFollowUpState(mineBranch);
        mineBranch.AddState(burn, SIM_COND(Queen, !self.HasAmalgamDied()));
        mineBranch.AddState(head, SIM_COND(Queen, self.HasAmalgamDied()));
        burn.SetFollowUpState(burnBranch);
        burnBranch.AddState(burn, SIM_COND(Queen, !self.HasAmalgamDied()));
        burnBranch.AddState(head, SIM_COND(Queen, self.HasAmalgamDied()));
        head.SetFollowUpState(execution);
        execution.SetFollowUpState(enrage);
        enrage.SetFollowUpState(head);
        b.Add(puppet); b.Add(mine); b.Add(burn); b.Add(burnBranch); b.Add(mineBranch); b.Add(head); b.Add(execution); b.Add(enrage);
        return b.Build(puppet);
    }
};
}  // namespace sts2
