#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Splash : CardModel {
    SIM_MODEL(Splash, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        // UnlockState.all / ModelDb.AllCharacters order: Ironclad, Silent,
        // Regent, Necrobinder, Defect. Single-player actor exclusion follows
        // the source's `if (poolCount > 1) Remove(ownPool)` rule.
        StaticVec<CardPoolId, 8> all_pools;
        all_pools.push_back(CardPoolId::IRONCLAD_CARD_POOL);
        all_pools.push_back(CardPoolId::SILENT_CARD_POOL);
        all_pools.push_back(CardPoolId::REGENT_CARD_POOL);
        all_pools.push_back(CardPoolId::NECROBINDER_CARD_POOL);
        all_pools.push_back(CardPoolId::DEFECT_CARD_POOL);
        StaticVec<CardPoolId, 8> pools;
        const CardPoolId own_pool = sts2::data(Owner().Character()).card_pool;
        for (CardPoolId pool : all_pools) if (all_pools.size() <= 1 || pool != own_pool) pools.push_back(pool);

        CardList cards = CardFactory::GetForCombat(Owner(), pools, 3,
                                                   Owner().RunState().Rng().CombatCardGeneration(), CardType::Attack, true);
        if (CurrentUpgradeLevel() > 0) for (CardModel card : cards) CardCmd::Upgrade(card);
        CardModel chosen = CardSelectCmd::FromChooseACardScreen(ctx, cards, Owner(), true);
        if (chosen) {
            chosen.SetToFreeThisTurn();
            CardPileCmd::AddGeneratedCardToCombat(chosen, PileType::Hand, Owner());
        }
    }

};

}  // namespace sts2
