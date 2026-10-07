// Models/Monsters/KinFollower.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct KinFollower : MonsterModel {
    SIM_MODEL(KinFollower, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    bool StartsWithDance() const { return extra(0) != 0; }
    void SetStartsWithDance(bool value) const { extra(0) = value ? 1 : 0; }

    int32_t QuickSlashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 5, 5); }
    int32_t BoomerangDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 2, 2); }
    int32_t DanceStrength() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 2); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::MINION_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    void QuickSlashMove(const CreatureList&) const { DamageCmd::Attack(QuickSlashDamage()).FromMonster(*this).Execute(nullptr); }
    void BoomerangMove(const CreatureList&) const {
        DamageCmd::Attack(BoomerangDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
    }
    void PowerDanceMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(DanceStrength()), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState slash = b.Move("QUICK_SLASH_MOVE", SIM_MOVE(KinFollower, QuickSlashMove), SingleAttackIntent(QuickSlashDamage()));
        MoveState boomerang = b.Move("BOOMERANG_MOVE", SIM_MOVE(KinFollower, BoomerangMove), MultiAttackIntent(BoomerangDamage(), 2));
        MoveState dance = b.Move("POWER_DANCE_MOVE", SIM_MOVE(KinFollower, PowerDanceMove), BuffIntent());
        slash.SetFollowUpState(boomerang);
        boomerang.SetFollowUpState(dance);
        dance.SetFollowUpState(slash);
        b.Add(slash);
        b.Add(boomerang);
        b.Add(dance);
        return b.Build(StartsWithDance() ? dance : slash);
    }
};

}  // namespace sts2
