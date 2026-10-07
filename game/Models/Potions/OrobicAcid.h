#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct OrobicAcid : PotionModel {
    SIM_MODEL(OrobicAcid, PotionModel)

    void OnUse(PlayerChoiceContext&, Creature target) const {
        if (!target) return;
        Player player = target.Player();
        CardList cards;
        CardPoolId pool = sts2::data(player.Character()).card_pool;
        Rng& rng = player.RunState().Rng().CombatCardGeneration();
        cards.push_back(CardFactory::GetForCombat(player, pool, 1, rng, CardType::Attack, true).front());
        cards.push_back(CardFactory::GetForCombat(player, pool, 1, rng, CardType::Skill, true).front());
        cards.push_back(CardFactory::GetForCombat(player, pool, 1, rng, CardType::Power, true).front());
        for (CardModel card : cards) card.SetToFreeThisTurn();
        CardPileCmd::AddGeneratedCardsToCombat(cards, PileType::Hand, Owner());
    }
};

}  // namespace sts2
