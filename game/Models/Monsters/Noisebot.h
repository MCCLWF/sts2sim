// Models/Monsters/Noisebot.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2 {

struct Noisebot : MonsterModel {
    SIM_MODEL(Noisebot, MonsterModel)

    SIM_NOT_PORTED(GenerateAnimator);

    void AfterAddedToRoom() const {}
    void NoiseMove(const CreatureList& targets) const {
        for (sts2::Creature target : targets) {
            Player player = target.Player() ? target.Player() : target.PetOwner();
            if (!player) continue;
            CardPileCmd::AddGeneratedCardToCombat(CombatState().CreateCard(CardId::DAZED, player), PileType::Discard,
                                                   {});
            CardPileCmd::AddGeneratedCardToCombat(CombatState().CreateCard(CardId::DAZED, player), PileType::Draw,
                                                   {}, CardPilePosition::Random);
        }
    }

    MoveMachineDef GenerateMoveStateMachine() const {
        MoveMachineBuilder b;
        MoveState noise = b.Move("NOISE_MOVE", SIM_MOVE(Noisebot, NoiseMove), StatusIntent(2));
        noise.SetFollowUpState(noise);
        b.Add(noise);
        return b.Build(noise);
    }
};

}  // namespace sts2
