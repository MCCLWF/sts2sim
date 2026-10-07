// Models/Monsters/Stabbot.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Stabbot : MonsterModel {
    SIM_MODEL(Stabbot, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    int32_t StabDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 12, 11); }
    void AfterAddedToRoom() const {}
    void StabMove(const CreatureList& targets) const {
        DamageCmd::Attack(StabDamage()).FromMonster(*this).Execute(nullptr);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::FRAIL_POWER, ctx, targets, Dec(1), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState stab = b.Move("STAB_MOVE", SIM_MOVE(Stabbot, StabMove), SingleAttackIntent(StabDamage()), DebuffIntent());
        stab.SetFollowUpState(stab);
        b.Add(stab);
        return b.Build(stab);
    }
};

}  // namespace sts2
