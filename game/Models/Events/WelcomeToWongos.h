// Models/Events/WelcomeToWongos.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/Canonical.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct WelcomeToWongos : EventModel {
    SIM_MODEL(WelcomeToWongos, EventModel)

    // _featuredItem is canonical until the featured option is purchased.
    RelicId FeaturedItem() const { return static_cast<RelicId>(extra(0)); }

    EventOptionList GenerateInitialOptions() const {
        Player owner = Owner();
        RelicId featured = RelicFactory::PullNextRelicFromFront(owner, RelicRarity::Rare,
            [this](RelicId id) { return CanonicalRelic(sim_, id).get().IsAllowedInShops(); });
        extra(0) = static_cast<int32_t>(featured);
        EventOptionList list;
        list.push_back(Dec(owner.Gold()) >= DynamicVars()["BargainBinCost"].BaseValue()
            ? EventOption(SIM_OPTION(WelcomeToWongos, BuyBargainBin), InitialOptionKey("BARGAIN_BIN"))
            : EventOption(0, InitialOptionKey("BARGAIN_BIN_LOCKED")));
        list.push_back(Dec(owner.Gold()) >= DynamicVars()["FeaturedItemCost"].BaseValue()
            ? EventOption(SIM_OPTION(WelcomeToWongos, BuyFeaturedItem), InitialOptionKey("FEATURED_ITEM"))
            : EventOption(0, InitialOptionKey("FEATURED_ITEM_LOCKED")));
        list.push_back(Dec(owner.Gold()) >= DynamicVars()["MysteryBoxCost"].BaseValue()
            ? EventOption(SIM_OPTION(WelcomeToWongos, BuyMysteryBox), InitialOptionKey("MYSTERY_BOX"))
            : EventOption(0, InitialOptionKey("MYSTERY_BOX_LOCKED")));
        list.push_back(EventOption(SIM_OPTION(WelcomeToWongos, Leave), InitialOptionKey("LEAVE")));
        return list;
    }

    LocKey CheckObtainWongoBadge(int32_t pointsEarned) const {
        const int32_t progress = sim_->g->run.progress_wongo_points;
        const int32_t current = progress % 2000 + pointsEarned;
        const int32_t total = progress + pointsEarned;
        DynamicVars()["WongoPointAmount"].SetBaseValue(Dec(current));
        DynamicVars()["RemainingWongoPointAmount"].SetBaseValue(Dec(2000 - current));
        DynamicVars()["TotalWongoBadgeAmount"].SetBaseValue(Dec(total / 2000));
        Owner().st().wongo_points_earned = pointsEarned;
        if (current >= 2000) {
            RelicCmd::Obtain(RelicId::WONGO_CUSTOMER_APPRECIATION_BADGE, Owner());
            return L10NLookup("WELCOME_TO_WONGOS.pages.AFTER_BUY_RECEIVE_BADGE.description");
        }
        if (total / 2000 > 0)
            return L10NLookup("WELCOME_TO_WONGOS.pages.AFTER_BUY_BADGE_COUNTER.description");
        return L10NLookup("WELCOME_TO_WONGOS.pages.AFTER_BUY.description");
    }

    void BuyBargainBin(const EventOptionState&) const {
        PlayerCmd::LoseGold(DynamicVars()["BargainBinCost"].BaseValue(), Owner(), GoldLossType::Spent);
        RelicId relic = RelicFactory::PullNextRelicFromFront(Owner(), RelicRarity::Common,
            [this](RelicId id) { return CanonicalRelic(sim_, id).get().IsAllowedInShops(); });
        RelicCmd::Obtain(relic, Owner());
        SetEventFinished(CheckObtainWongoBadge(32));
    }

    void BuyFeaturedItem(const EventOptionState&) const {
        PlayerCmd::LoseGold(DynamicVars()["FeaturedItemCost"].BaseValue(), Owner(), GoldLossType::Spent);
        if (FeaturedItem() == RelicId::NONE) throw GameError("Wongo featured relic is missing");
        RelicCmd::Obtain(FeaturedItem(), Owner());
        SetEventFinished(CheckObtainWongoBadge(16));
    }

    void BuyMysteryBox(const EventOptionState&) const {
        PlayerCmd::LoseGold(DynamicVars()["MysteryBoxCost"].BaseValue(), Owner(), GoldLossType::Spent);
        RelicCmd::Obtain(RelicId::WONGOS_MYSTERY_TICKET, Owner());
        SetEventFinished(CheckObtainWongoBadge(8));
    }

    void Leave(const EventOptionState&) const {
        CardList upgraded;
        for (CardModel card : Owner().Deck().Cards()) if (card.CurrentUpgradeLevel() > 0) upgraded.push_back(card);
        CardModel selected = Rng().NextItem(upgraded);
        if (selected) CardCmd::Downgrade(selected);
        SetEventFinished(L10NLookup("WELCOME_TO_WONGOS.pages.LEAVE.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return runState.CurrentActIndex() == 1 && all_players(runState, [](Player p) { return p.Gold() >= 100; }); }
};

}  // namespace sts2
