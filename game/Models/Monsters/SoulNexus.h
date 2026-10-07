// Models/Monsters/SoulNexus.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct SoulNexus : MonsterModel {
    SIM_MODEL(SoulNexus, MonsterModel)

    bool ShouldFadeAfterDeath() const { return false; }
    void AfterAddedToRoom() const {}
    void BeforeRemovedFromRoom() const {}

    int32_t SoulBurnDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 31, 29); }
    int32_t MaelstromDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 7, 6); }
    int32_t MaelstromRepeat() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 4); }
    int32_t DrainLifeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 19, 18); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState burn = b.Move("SOUL_BURN_MOVE", SIM_MOVE(SoulNexus, SoulBurnMove), SingleAttackIntent(SoulBurnDamage()));
        MoveState maelstrom = b.Move("MAELSTROM_MOVE", SIM_MOVE(SoulNexus, MaelstromMove), MultiAttackIntent(MaelstromDamage(), MaelstromRepeat()));
        MoveState drain = b.Move("DRAIN_LIFE_MOVE", SIM_MOVE(SoulNexus, DrainLifeMove), SingleAttackIntent(DrainLifeDamage()), DebuffIntent(/*strong=*/true));
        RandomBranchState random = b.RandomBranch("RAND");
        burn.SetFollowUpState(random);
        maelstrom.SetFollowUpState(random);
        drain.SetFollowUpState(random);
        random.AddBranch(burn, MoveRepeatType::CannotRepeat, 1.0f);
        random.AddBranch(maelstrom, MoveRepeatType::CannotRepeat, 1.0f);
        random.AddBranch(drain, MoveRepeatType::CannotRepeat, 1.0f);
        b.Add(burn);
        b.Add(maelstrom);
        b.Add(drain);
        b.Add(random);
        return b.Build(burn);
    }

    void SoulBurnMove(const CreatureList&) const { DamageCmd::Attack(SoulBurnDamage()).FromMonster(*this).Execute(nullptr); }
    void MaelstromMove(const CreatureList&) const { DamageCmd::Attack(MaelstromDamage()).WithHitCount(MaelstromRepeat()).FromMonster(*this).Execute(nullptr); }
    void DrainLifeMove(const CreatureList& targets) const {
        DamageCmd::Attack(DrainLifeDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, Dec(2), Creature(), CardModel{});
    }
};

}  // namespace sts2
