// Models/Monsters/Ovicopter.cs
#pragma once

#include <string_view>

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Ovicopter : MonsterModel {
    SIM_MODEL(Ovicopter, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);
    SIM_NOT_PORTED(get_TakeDamageSfxType);

    int32_t SmashDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 17, 16);
    }
    int32_t TenderizerDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 8, 7);
    }
    int32_t NutritionalPasteStrengthAmount() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3);
    }

    bool CanLay() const {
        int32_t living = 0;
        for (sts2::Creature teammate : CombatState().GetTeammatesOf(Creature())) {
            if (teammate.IsAlive()) ++living;
        }
        return living <= 3;
    }

    void AfterAddedToRoom() const {}
    void BeforeRemovedFromRoom() const {}

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState layEggs = b.Move("LAY_EGGS_MOVE", SIM_MOVE(Ovicopter, LayEggsMove), SummonIntent());
        MoveState smash = b.Move("SMASH_MOVE", SIM_MOVE(Ovicopter, SmashMove), SingleAttackIntent(SmashDamage()));
        MoveState tenderizer = b.Move("TENDERIZER_MOVE", SIM_MOVE(Ovicopter, TenderizerMove),
                                     SingleAttackIntent(TenderizerDamage()), DebuffIntent());
        MoveState paste = b.Move("NUTRITIONAL_PASTE_MOVE", SIM_MOVE(Ovicopter, NutritionalPasteMove), BuffIntent());
        ConditionalBranchState summonBranch = b.ConditionalBranch("SUMMON_BRANCH_STATE");
        summonBranch.AddState(layEggs, SIM_COND(Ovicopter, self.CanLay()));
        summonBranch.AddState(paste, SIM_COND(Ovicopter, !self.CanLay()));
        layEggs.SetFollowUpState(smash);
        paste.SetFollowUpState(smash);
        smash.SetFollowUpState(tenderizer);
        tenderizer.SetFollowUpState(summonBranch);
        b.Add(paste);
        b.Add(layEggs);
        b.Add(smash);
        b.Add(tenderizer);
        b.Add(summonBranch);
        return b.Build(layEggs);
    }

    void LayEggsMove(const CreatureList&) const {
        for (int32_t i = 0; i < 3; ++i) {
            std::string_view freeEggSlot;
            EncounterModel encounter(sim_);
            for (std::string_view slot : encounter.Slots()) {
                if (!slot.starts_with("egg")) continue;
                bool free = true;
                for (sts2::Creature enemy : CombatState().Enemies()) free &= enemy.SlotName() != slot;
                if (free) freeEggSlot = slot;  // source uses LastOrDefault over Slots
            }
            if (freeEggSlot.empty()) break;
            sts2::Creature egg = CreatureCmd::Add(MonsterModel::ToMutable(sim_, MonsterId::TOUGH_EGG),
                                                  CombatState(), CombatSide::Enemy, freeEggSlot);
            PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
            PowerCmd::Apply(PowerId::MINION_POWER, ctx, egg, Dec(1), Creature(), CardModel{});
        }
    }
    void NutritionalPasteMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(NutritionalPasteStrengthAmount()),
                        Creature(), CardModel{});
    }
    void SmashMove(const CreatureList&) const { DamageCmd::Attack(SmashDamage()).FromMonster(*this).Execute(nullptr); }
    void TenderizerMove(const CreatureList& targets) const {
        DamageCmd::Attack(TenderizerDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
