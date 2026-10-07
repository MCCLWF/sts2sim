// Models/Events/TheFutureOfPotions.cs
#pragma once

#include "game/Commands/PotionCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct TheFutureOfPotions : EventModel {
    SIM_MODEL(TheFutureOfPotions, EventModel)
    void BeforeEventStarted(bool isPreFinished) const {
        (void)isPreFinished;
        Owner().SetCanUseOrRemovePotions(false);
    }
    EventOptionList GenerateInitialOptions() const {
        StaticVec<PotionModel, kMaxPotionSlots> potions = Owner().Potions();
        if (extra(7) == 0) {
            for (size_t i = 0; i < potions.size(); ++i) {
                StaticVec<CardType, 3> types;
                types.push_back(CardType::Attack);
                types.push_back(CardType::Skill);
                PotionRarity rarity = potions[i].Rarity();
                if (rarity != PotionRarity::Common && rarity != PotionRarity::Token)
                    types.push_back(CardType::Power);
                if (i < 3) extra(static_cast<int>(i)) = static_cast<int32_t>(Rng().NextItem(types));
                else (void)Rng().NextItem(types);
            }
            extra(7) = 1;
        }
        EventOptionList list;
        size_t count = std::min<size_t>(3, potions.size());
        for (size_t i = 0; i < count; ++i)
            list.push_back(EventOption(SIM_OPTION(TheFutureOfPotions, Trade), InitialOptionKey("POTION"))
                               .WithArg(static_cast<int32_t>(i)).ThatHasDynamicTitle());
        return list;
    }

    static CardRarity CardRarityFor(PotionRarity rarity) {
        switch (rarity) {
        case PotionRarity::Rare:
        case PotionRarity::Event: return CardRarity::Rare;
        case PotionRarity::Uncommon: return CardRarity::Uncommon;
        case PotionRarity::Common:
        case PotionRarity::Token: return CardRarity::Common;
        default: throw GameError("TheFutureOfPotions: invalid potion rarity");
        }
    }

    void Trade(const EventOptionState& option) const {
        StaticVec<PotionModel, kMaxPotionSlots> potions = Owner().Potions();
        int32_t index = option.arg;
        if (index < 0 || index >= static_cast<int32_t>(potions.size()) || index >= 3)
            throw GameError("TheFutureOfPotions: invalid potion option");
        PotionModel potion = potions[static_cast<size_t>(index)];
        CardRarity rarity = CardRarityFor(potion.Rarity());
        CardType type = static_cast<CardType>(extra(index));
        PotionCmd::Discard(potion);
        CardCreationOptionsState options = CardCreationOptions::ForNonCombatWithUniformOdds(
            {sts2::data(Owner().Character()).card_pool});
        options.required_rarity = rarity;
        options.required_type = type;
        options.flags = options.flags | CardCreationFlags::NoRarityModification |
                        CardCreationFlags::NoCardPoolModifications;
        Reward reward = Reward::NewCardReward(Owner(), options, 3);
        reward.st().upgrade_after_generated = true;
        StaticVec<Reward, 8> rewards;
        rewards.push_back(reward);
        RewardsCmd::OfferCustom(Owner(), rewards);
        SetEventFinished(L10NLookup("THE_FUTURE_OF_POTIONS.pages.DONE.description"));
    }
    void OnEventFinished() const { Owner().SetCanUseOrRemovePotions(true); }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return all_players(runState, [](Player p) { return p.Potions().size() >= 2; }); }
};

}  // namespace sts2
