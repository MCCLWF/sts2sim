// Models/Relics/LastingCandy.cs
#pragma once

#include "game/Factories/CardFactory.h"
#include "game/Models/RelicModel.h"
#include "game/Rewards/Reward.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct LastingCandy : RelicModel {
    SIM_MODEL(LastingCandy, RelicModel)
    bool ShowCounter() const { return true; }
    int32_t DisplayAmount() const { return extra(0) % 2; }
    bool IsAllowed(RunState runState) const { return IsBeforeAct3TreasureChest(runState); }
    bool IsInTriggeringCombat() const { return extra(0) > 0 && extra(0) % 2 == 1; }

    bool TryModifyCardRewardOptions(Player player, CardList& rewardOptions,
                                    const CardCreationOptionsState& creationOptions) const {
        if (player != Owner() || creationOptions.source != CardCreationSource::Encounter ||
            !IsInTriggeringCombat() || !has_flag(creationOptions.flags, CardCreationFlags::IsCardReward) ||
            !has_flag(creationOptions.flags, CardCreationFlags::IsFromCombat)) return false;
        if (creationOptions.required_type != CardType::None && creationOptions.required_type != CardType::Power)
            return false;
        StaticVec<CardId, 8> existing;
        for (CardModel card : rewardOptions) existing.push_back(card.CardId_());
        // Source constructs a new record, keeping only pools, rarity odds,
        // and its composed filter. Parent flags and RNG override do not carry
        // into the extra Power reward.
        CardCreationOptionsState options{};
        options.pools = creationOptions.pools;
        options.rarity_odds = creationOptions.rarity_odds;
        options.required_rarity = creationOptions.required_rarity;
        options.filter = creationOptions.filter;
        options.excluded_ids = creationOptions.excluded_ids;
        options.source = CardCreationSource::Other;
        options.required_type = CardType::Power;
        options.flags = CardCreationFlags::NoModifyHooks | CardCreationFlags::NoCardPoolModifications;
        bool hasAny = false, hasUnique = false;
        for (CardId id : CardCreationOptions::GetPossibleCards(options, player)) {
            hasAny = true;
            if (!existing.contains(id)) hasUnique = true;
        }
        if (!hasAny) return false;
        if (hasUnique) {
            for (CardId id : existing)
                if (!options.excluded_ids.contains(id)) options.excluded_ids.push_back(id);
        }
        CardList generated = CardFactory::CreateForReward(player, 1, options);
        if (generated.empty()) return false;
        rewardOptions.push_back(generated[0]);
        return true;
    }
    void BeforeCombatRewardOffered(RewardsSet rewards, CombatRoom) const {
        if (rewards.Player() != Owner()) return;
        for (size_t i = 0; i < rewards.Rewards().size(); ++i) {
            if (rewards.Rewards()[i].Type() == RewardType::Card) { ++extra(0); return; }
        }
    }
};

}  // namespace sts2
