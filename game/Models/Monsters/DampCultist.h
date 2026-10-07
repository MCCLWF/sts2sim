// Models/Monsters/DampCultist.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct DampCultist : MonsterModel {
    SIM_MODEL(DampCultist, MonsterModel)

    int32_t DarkStrikeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 3, 1); }
    int32_t IncantationAmount() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 6, 5); }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState incantation = b.Move("INCANTATION_MOVE", SIM_MOVE(DampCultist, IncantationMove), BuffIntent());
        MoveState strike = b.Move("DARK_STRIKE_MOVE", SIM_MOVE(DampCultist, DarkStrikeMove), SingleAttackIntent(DarkStrikeDamage()));
        incantation.SetFollowUpState(strike);
        strike.SetFollowUpState(strike);
        b.Add(incantation);
        b.Add(strike);
        return b.Build(incantation);
    }

    void IncantationMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::RITUAL_POWER, ctx, Creature(), Dec(IncantationAmount()), Creature(), CardModel{});
    }

    void DarkStrikeMove(const CreatureList&) const {
        DamageCmd::Attack(DarkStrikeDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
