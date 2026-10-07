// Models/Relics/ScrollBoxes.cs
#pragma once
#include "game/Factories/CardFactory.h"
#include "game/Models/Canonical.h"
#include "game/Models/RelicModel.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Runs/CardCreationOptions.h"
namespace sts2 {
struct ScrollBoxes : RelicModel {
    SIM_MODEL(ScrollBoxes, RelicModel)
    bool IsAllowedAtNeow(Player player) const { return CanGenerateBundles(player) && IsAllowedAtNeow_Default(player); }
    static bool CanGenerateBundles(Player player) {
        int common = 0, uncommon = 0;
        Sim* sim = &player.sim();
        for (CardId id : CardFactory::GetUnlockedCards(sts2::data(player.Character()).card_pool, player)) {
            CanonicalCard card(sim, id);
            if (card.get().Rarity() == CardRarity::Common) ++common;
            if (card.get().Rarity() == CardRarity::Uncommon) ++uncommon;
        }
        return common >= 4 && uncommon >= 2;
    }
    void AfterObtained() const {
        CardPoolId pool = sts2::data(Owner().Character()).card_pool;
        StaticVec<CardId, 640> source, source2, used;
        auto cardsOfRarity = [&](CardRarity rarity) {
            CardCreationOptionsState options = CardCreationOptions::ForNonCombatWithUniformOdds({pool});
            options.required_rarity = rarity;
            CardCreationOptions::WithFlags(options, CardCreationFlags::NoRarityModification);
            options = Hook::ModifyCardRewardCreationOptions(Owner().RunState(), Owner(), options);
            return CardCreationOptions::GetPossibleCards(options, Owner());
        };
        source = cardsOfRarity(CardRarity::Common);
        source2 = cardsOfRarity(CardRarity::Uncommon);

        StaticVec<CardId, 3> bundleIds[2];
        auto& rewards = Owner().PlayerRng().Rewards();
        const bool isDefect = Owner().Character() == CharacterId::DEFECT;
        for (int b = 0; b < 2; ++b) {
            if (isDefect && rewards.NextInt(100) < 1) {
                for (int i = 0; i < 3; ++i) bundleIds[b].push_back(CardId::CLAW);
                continue;
            }
            StaticVec<CardId, 640> items;
            for (CardId id : source) if (!used.contains(id)) items.push_back(id);
            for (int i = 0; i < 2; ++i) {
                int32_t k = rewards.next_item_index(static_cast<int32_t>(items.size()));
                if (k >= 0) {
                    CardId id = items[static_cast<size_t>(k)];
                    used.push_back(id);
                    bundleIds[b].push_back(id);
                    items.remove(id);
                }
            }
            StaticVec<CardId, 640> uncommonItems;
            for (CardId id : source2) if (!used.contains(id)) uncommonItems.push_back(id);
            int32_t k = rewards.next_item_index(static_cast<int32_t>(uncommonItems.size()));
            if (k >= 0) {
                CardId id = uncommonItems[static_cast<size_t>(k)];
                used.push_back(id);
                bundleIds[b].push_back(id);
            }
        }
        std::vector<CardList> bundles;
        for (const StaticVec<CardId, 3>& ids : bundleIds) {
            CardList bundle;
            for (CardId id : ids) bundle.push_back(Owner().RunState().CreateCard(id, Owner()));
            bundles.push_back(bundle);
        }
        CardList chosen = CardSelectCmd::FromChooseABundleScreen(Owner(), bundles);
        for (CardModel c : chosen) CardPileCmd::Add(c, PileType::Deck);
    }
};
}
