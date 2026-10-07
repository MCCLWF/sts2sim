// Models/Monsters/CalcifiedCultist.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct CalcifiedCultist : MonsterModel {
    SIM_MODEL(CalcifiedCultist, MonsterModel)

    int32_t DarkStrikeDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 11, 9); }
    int32_t IncantationAmount() const { return 2; }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState incantation = b.Move("INCANTATION_MOVE", SIM_MOVE(CalcifiedCultist, IncantationMove), BuffIntent());
        MoveState strike = b.Move("DARK_STRIKE_MOVE", SIM_MOVE(CalcifiedCultist, DarkStrikeMove), SingleAttackIntent(DarkStrikeDamage()));
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
