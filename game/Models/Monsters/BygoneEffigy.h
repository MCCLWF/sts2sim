// Models/Monsters/BygoneEffigy.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct BygoneEffigy : MonsterModel {
    SIM_MODEL(BygoneEffigy, MonsterModel)

    int32_t SlashDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 15, 13); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::SLOW_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState sleep = b.Move("SLEEP_MOVE", SIM_MOVE(BygoneEffigy, InitialSleepMove), SleepIntent());
        MoveState wake = b.Move("WAKE_MOVE", SIM_MOVE(BygoneEffigy, WakeMove), BuffIntent());
        MoveState sleep2 = b.Move("SLEEP_MOVE_2", SIM_MOVE(BygoneEffigy, SleepMove), SleepIntent());
        MoveState slashes = b.Move("SLASHES_MOVE", SIM_MOVE(BygoneEffigy, SlashMove), SingleAttackIntent(SlashDamage()));
        sleep.SetFollowUpState(wake);
        wake.SetFollowUpState(slashes);
        sleep2.SetFollowUpState(slashes);
        slashes.SetFollowUpState(slashes);
        b.Add(sleep);
        b.Add(wake);
        b.Add(sleep2);
        b.Add(slashes);
        return b.Build(sleep);
    }

    void InitialSleepMove(const CreatureList&) const {}
    void SleepMove(const CreatureList&) const {}

    void WakeMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Creature(), Dec(10), Creature(), CardModel{});
    }

    void SlashMove(const CreatureList&) const {
        DamageCmd::Attack(SlashDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
