// Models/Events/RanwidTheElder.cs (v0.111.0).
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct RanwidTheElder : EventModel {
    SIM_MODEL(RanwidTheElder, EventModel)
    void BeforeEventStarted(bool) const { Owner().SetCanUseOrRemovePotions(false); }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList result;
        PotionModel potion = Rng().NextItem(Owner().Potions());
        if (potion)
            result.push_back(EventOption(SIM_OPTION(RanwidTheElder, GivePotion), InitialOptionKey("POTION"))
                                 .WithArg(potion.Index()).ThatHasDynamicTitle());
        else
            result.push_back(EventOption(0, InitialOptionKey("POTION_LOCKED")));

        result.push_back(EventOption(SIM_OPTION(RanwidTheElder, GiveGold), InitialOptionKey("GOLD")));

        StaticVec<RelicModel, kMaxRelics> tradable;
        for (RelicModel relic : Owner().Relics()) if (event_allowed::IsTradable(relic)) tradable.push_back(relic);
        RelicModel relic = Rng().NextItem(tradable);
        if (relic)
            result.push_back(EventOption(SIM_OPTION(RanwidTheElder, GiveRelic), InitialOptionKey("RELIC"))
                                 .WithArg(relic.Index()).ThatHasDynamicTitle());
        else
            result.push_back(EventOption(0, InitialOptionKey("RELIC_LOCKED")));
        return result;
    }

    void GivePotion(const EventOptionState& option) const {
        PotionCmd::Discard(PotionModel(&sim(), static_cast<uint32_t>(option.arg)));
        RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
        SetEventFinished(L10NLookup("RANWID_THE_ELDER.pages.POTION.description"));
    }

    void GiveGold(const EventOptionState&) const {
        PlayerCmd::LoseGold(DynamicVars().Gold().IntValue(), Owner(), GoldLossType::Spent);
        RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
        SetEventFinished(L10NLookup("RANWID_THE_ELDER.pages.GOLD.description"));
    }

    void GiveRelic(const EventOptionState& option) const {
        RelicCmd::Remove(RelicModel(&sim(), static_cast<uint32_t>(option.arg)));
        for (int i = 0; i < 2; ++i)
            RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
        SetEventFinished(L10NLookup("RANWID_THE_ELDER.pages.RELIC.description"));
    }

    void OnEventFinished() const { Owner().SetCanUseOrRemovePotions(true); }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { if (runState.CurrentActIndex() == 0) return false; return all_players(runState, [](Player p) { int n = 0; for (RelicModel r : p.Relics()) if (event_allowed::IsTradable(r)) ++n; return n > 0 && p.Gold() >= 100 && !p.Potions().empty(); }); }
};

}  // namespace sts2
