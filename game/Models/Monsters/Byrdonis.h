// Models/Monsters/Byrdonis.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Byrdonis : MonsterModel {
    SIM_MODEL(Byrdonis, MonsterModel)

    int32_t PeckDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 4, 3); }
    int32_t PeckRepeat() const { return 3; }
    int32_t SwoopDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 19, 17); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::TERRITORIAL_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState peck = b.Move("PECK_MOVE", SIM_MOVE(Byrdonis, PeckMove), MultiAttackIntent(PeckDamage(), PeckRepeat()));
        MoveState swoop = b.Move("SWOOP_MOVE", SIM_MOVE(Byrdonis, SwoopMove), SingleAttackIntent(SwoopDamage()));
        peck.SetFollowUpState(swoop);
        swoop.SetFollowUpState(peck);
        b.Add(swoop);
        b.Add(peck);
        return b.Build(swoop);
    }

    void PeckMove(const CreatureList& targets) const { DamageCmd::Attack(PeckDamage()).WithHitCount(PeckRepeat()).FromMonster(*this).Execute(nullptr); }
    void SwoopMove(const CreatureList& targets) const { DamageCmd::Attack(SwoopDamage()).FromMonster(*this).Execute(nullptr); }
};

}  // namespace sts2
