// Models/Monsters/DevotedSculptor.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct DevotedSculptor : MonsterModel {
    SIM_MODEL(DevotedSculptor, MonsterModel)

    int32_t SavageDamage() const {
        return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 15, 12);
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState incantation = b.Move("FORBIDDEN_INCANTATION_MOVE", SIM_MOVE(DevotedSculptor, ForbiddenIncantationMove), BuffIntent());
        MoveState savage = b.Move("SAVAGE_MOVE", SIM_MOVE(DevotedSculptor, SavageMove), SingleAttackIntent(SavageDamage()));
        incantation.SetFollowUpState(savage);
        savage.SetFollowUpState(savage);
        b.Add(incantation);
        b.Add(savage);
        return b.Build(incantation);
    }

    void ForbiddenIncantationMove(const CreatureList&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::RITUAL_POWER, ctx, Creature(), Dec(9), sts2::Creature{}, CardModel{});  // applier: null
    }

    void SavageMove(const CreatureList&) const {
        DamageCmd::Attack(SavageDamage()).FromMonster(*this).Execute(nullptr);
    }
};

}  // namespace sts2
