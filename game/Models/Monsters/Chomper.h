// Models/Monsters/Chomper.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Chomper : MonsterModel {
    SIM_MODEL(Chomper, MonsterModel)

    bool ScreamFirst() const { return extra(0) != 0; }
    void SetScreamFirst(bool value) const { extra(0) = value; }
    int32_t ClampDamage() const { return AscensionHelper::GetValueIfAscension(sim(), AscensionLevel::DeadlyEnemies, 9, 8); }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::ARTIFACT_POWER, ctx, Creature(), Dec(2), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState clamp = b.Move("CLAMP_MOVE", SIM_MOVE(Chomper, ClampMove), MultiAttackIntent(ClampDamage(), 2));
        MoveState screech = b.Move("SCREECH_MOVE", SIM_MOVE(Chomper, ScreechMove), StatusIntent(3));
        clamp.SetFollowUpState(screech);
        screech.SetFollowUpState(clamp);
        b.Add(clamp);
        b.Add(screech);
        return b.Build(ScreamFirst() ? screech : clamp);
    }

    void ClampMove(const CreatureList&) const {
        DamageCmd::Attack(ClampDamage()).WithHitCount(2).FromMonster(*this).Execute(nullptr);
    }

    void ScreechMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::DAZED, targets, PileType::Discard, 3, Player{});
    }
};

}  // namespace sts2
