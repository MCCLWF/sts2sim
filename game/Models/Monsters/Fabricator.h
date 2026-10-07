// Models/Monsters/Fabricator.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/EncounterModel.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Fabricator : MonsterModel {
    SIM_MODEL(Fabricator, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t FabricatingStrikeDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 21, 18);
    }
    int32_t DisintegrateDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 13, 11);
    }
    bool ShouldFadeAfterDeath() const { return false; }

    bool CanFabricate() const {
        int32_t living = 0;
        for (sts2::Creature ally : CombatState().GetTeammatesOf(Creature())) if (ally.IsAlive()) ++living;
        return living < 4;
    }
    bool HasLastSpawned() const { return extra(1) != 0; }
    MonsterId LastSpawned() const { return static_cast<MonsterId>(extra(0)); }
    void SetLastSpawned(MonsterId monster) const { extra(0) = static_cast<int32_t>(monster); extra(1) = 1; }

    MonsterId SelectBot(bool defensive) const {
        StaticVec<MonsterId, 2> choices;
        const MonsterId options[2] = {defensive ? MonsterId::GUARDBOT : MonsterId::ZAPBOT,
                                      defensive ? MonsterId::NOISEBOT : MonsterId::STABBOT};
        for (MonsterId id : options) if (!HasLastSpawned() || id != LastSpawned()) choices.push_back(id);
        MonsterId chosen = RunRng().MonsterAi().NextItem(choices);
        SetLastSpawned(chosen);
        return chosen;
    }
    void SpawnBot(bool defensive) const {
        EncounterModel encounter(sim_);
        std::string_view slot = encounter.GetNextSlot(CombatState());
        if (slot.empty()) return;
        sts2::Creature bot = CreatureCmd::Add(MonsterModel::ToMutable(sim_, SelectBot(defensive)), CombatState(),
                                              CombatSide::Enemy, slot);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::MINION_POWER, ctx, bot, Dec(1), Creature(), CardModel{});
    }
    void AfterAddedToRoom() const {}
    void FabricateMove(const CreatureList&) const { SpawnBot(true); SpawnBot(false); }
    void FabricatingStrikeMove(const CreatureList&) const {
        DamageCmd::Attack(FabricatingStrikeDamage()).FromMonster(*this).Execute(nullptr);
        SpawnBot(false);
    }
    void DisintegrateMove(const CreatureList&) const { DamageCmd::Attack(DisintegrateDamage()).FromMonster(*this).Execute(nullptr); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState fabricate = b.Move("FABRICATE_MOVE", SIM_MOVE(Fabricator, FabricateMove), SummonIntent());
        MoveState strike = b.Move("FABRICATING_STRIKE_MOVE", SIM_MOVE(Fabricator, FabricatingStrikeMove),
                                  SingleAttackIntent(FabricatingStrikeDamage()), SummonIntent());
        MoveState disintegrate = b.Move("DISINTEGRATE_MOVE", SIM_MOVE(Fabricator, DisintegrateMove),
                                        SingleAttackIntent(DisintegrateDamage()));
        RandomBranchState random = b.RandomBranch("RAND");
        random.AddBranch(fabricate, MoveRepeatType::CanRepeatForever, 1.0f);
        random.AddBranch(strike, MoveRepeatType::CanRepeatForever, 1.0f);
        ConditionalBranchState branch = b.ConditionalBranch("fabricateBranch");
        branch.AddState(random, SIM_COND(Fabricator, self.CanFabricate()));
        branch.AddState(disintegrate, SIM_COND(Fabricator, !self.CanFabricate()));
        fabricate.SetFollowUpState(branch);
        strike.SetFollowUpState(branch);
        disintegrate.SetFollowUpState(branch);
        b.Add(fabricate);
        b.Add(strike);
        b.Add(disintegrate);
        b.Add(branch);
        b.Add(random);
        return b.Build(branch);
    }
};

}  // namespace sts2
