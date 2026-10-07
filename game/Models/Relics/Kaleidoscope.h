// Models/Relics/Kaleidoscope.cs
#pragma once
#include <algorithm>
#include "game/Models/RelicModel.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Runs/CardCreationOptions.h"
namespace sts2 {
struct Kaleidoscope : RelicModel {
    SIM_MODEL(Kaleidoscope, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    bool IsAllowedAtNeow(Player player) const { return IsAllowedAtNeow_Default(player); }
    void AfterObtained() const {
        CardPoolId own = sts2::data(Owner().Character()).card_pool;
        CardPoolId all[] = {CardPoolId::DEFECT_CARD_POOL, CardPoolId::NECROBINDER_CARD_POOL, CardPoolId::REGENT_CARD_POOL, CardPoolId::SILENT_CARD_POOL, CardPoolId::IRONCLAD_CARD_POOL};
        StaticVec<Reward, 8> rewards;
        for (int n = 0; n < 2; ++n) {
            StaticVec<CardPoolId, 8> pools;
            for (CardPoolId p : all) if (p != own) pools.push_back(p);
            // ListExtensions.StableShuffle first sorts CardPoolModel values by
            // AbstractModel.CompareTo (their CARD_POOL entry IDs), then runs
            // the ordinary Fisher-Yates shuffle. Pool declaration order must
            // not affect Kaleidoscope's Niche RNG sequence.
            std::sort(pools.begin(), pools.end(), [](CardPoolId a, CardPoolId b) {
                return sts2::data(a).entry < sts2::data(b).entry;
            });
            Owner().RunState().Rng().Niche().Shuffle(pools);
            CardList cards;
            for (size_t i = 0; i < pools.size() && i < 3; ++i) {
                CardCreationOptionsState o = CardCreationOptions::Make({pools[i]}, CardCreationSource::Other, CardRarityOddsType::RegularEncounter);
                o.flags = o.flags | CardCreationFlags::NoCardPoolModifications;
                cards.push_back(CardFactory::CreateForReward(Owner(), 1, o)[0]);
            }
            rewards.push_back(Reward::NewCardReward(Owner(), cards));
        }
        RewardsCmd::OfferCustom(Owner(), rewards);
    }
};
}
