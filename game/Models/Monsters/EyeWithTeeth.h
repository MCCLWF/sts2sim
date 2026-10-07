// Models/Monsters/EyeWithTeeth.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/IllusionPower.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct EyeWithTeeth : MonsterModel {
    SIM_MODEL(EyeWithTeeth, MonsterModel)

    bool ShouldDisappearFromDoom() const { return false; }

    void AfterAddedToRoom() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::ILLUSION_POWER, ctx, Creature(), Dec(1), Creature(), CardModel{});
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState distract = b.Move("DISTRACT_MOVE", SIM_MOVE(EyeWithTeeth, DistractMove), StatusIntent(3));
        MoveState revive = b.Move("REVIVE_MOVE", SIM_MOVE(EyeWithTeeth, ReviveMove), HealIntent());
        distract.SetFollowUpState(distract);
        revive.SetFollowUpState(distract);
        revive.SetMustPerformOnceBeforeTransitioning(true);
        b.Add(distract); b.Add(revive);
        return b.Build(distract);
    }

    void DistractMove(const CreatureList& targets) const {
        CardPileCmd::AddToCombatAndPreview(CardId::DAZED, targets, PileType::Discard, 3, Player{});
    }

    void ReviveMove(const CreatureList&) const {
        IllusionPower power(Creature().GetPower(PowerId::ILLUSION_POWER));
        power.Revive();
    }
};

}  // namespace sts2
