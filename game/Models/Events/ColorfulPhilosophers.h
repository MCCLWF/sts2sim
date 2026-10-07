// Models/Events/ColorfulPhilosophers.cs (v0.111.0).
#pragma once

#include <array>

#include "game/Commands/RewardsCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct ColorfulPhilosophers : EventModel {
    SIM_MODEL(ColorfulPhilosophers, EventModel)
    EventOptionList GenerateInitialOptions() const {
        // CardPoolColorOrder, followed by removing the owner's own color.
        static constexpr std::array<CardPoolId, 5> kOrder = {
            CardPoolId::NECROBINDER_CARD_POOL, CardPoolId::IRONCLAD_CARD_POOL,
            CardPoolId::REGENT_CARD_POOL, CardPoolId::SILENT_CARD_POOL,
            CardPoolId::DEFECT_CARD_POOL};
        EventOptionList result;
        CardPoolId ownPool = sts2::data(Owner().Character()).card_pool;
        for (CardPoolId pool : kOrder) {
            if (pool == ownPool) continue;
            std::string key = InitialOptionKey(std::string(name(pool)).substr(0, std::string(name(pool)).find("_CARD_POOL")));
            result.push_back(EventOption(SIM_OPTION(ColorfulPhilosophers, OfferRewards), key)
                                 .WithArg(static_cast<int32_t>(pool)));
        }
        const size_t count = std::min<size_t>(3, result.size());
        while (result.size() > count) result.erase_at(static_cast<size_t>(Rng().NextInt(static_cast<int32_t>(result.size()))));
        return result;
    }

    void OfferRewards(const EventOptionState& option) const {
        const CardPoolId pool = static_cast<CardPoolId>(option.arg);
        StaticVec<Reward, 8> rewards;
        for (CardRarity rarity : {CardRarity::Common, CardRarity::Uncommon, CardRarity::Rare}) {
            CardCreationOptionsState options = CardCreationOptions::Make({pool}, CardCreationSource::Other,
                CardRarityOddsType::Uniform);
            options.required_rarity = rarity;
            options.flags = CardCreationFlags::NoRarityModification | CardCreationFlags::NoCardPoolModifications;
            rewards.push_back(Reward::NewCardReward(Owner(), options, DynamicVars().Cards().IntValue()));
        }
        RewardsCmd::OfferCustom(Owner(), rewards);
        SetEventFinished(L10NLookup("COLORFUL_PHILOSOPHERS.pages.DONE.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const {
        // UnlockState.all: every character card pool is available.
        return true;
    }
};

}  // namespace sts2
