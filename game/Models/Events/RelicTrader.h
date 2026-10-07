// Models/Events/RelicTrader.cs (v0.111.0).
#pragma once

#include "core/dotnet_sort.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct RelicTrader : EventModel {
    SIM_MODEL(RelicTrader, EventModel)

    // _ownedRelics and _newRelics are lazy C# fields. CalculateVars first
    // reads OwnedRelics, then NewRelics when at least one owned relic exists.
    // The POD event extras store owned pool refs and canonical new relic ids.
    int32_t& Count() const { return extra(0); }
    RelicModel Owned(int i) const { return RelicModel(&sim(), static_cast<RelicRef>(extra(1 + i))); }
    RelicId New(int i) const { return static_cast<RelicId>(extra(4 + i)); }

    void CalculateVars() const {
        StaticVec<RelicModel, kMaxRelics> owned;
        for (RelicModel relic : Owner().Relics()) if (event_allowed::IsTradable(relic)) owned.push_back(relic);
        dotnet_sort(owned.span(), [](RelicModel a, RelicModel b) {
            // Both IDs have the Relic category, so ModelId.CompareTo compares
            // the ordinal entry names.
            std::string_view x = a.data().entry, y = b.data().entry;
            return (x > y) - (x < y);
        });
        Rng().Shuffle(owned);
        Count() = static_cast<int32_t>(std::min<size_t>(3, owned.size()));
        for (int i = 0; i < Count(); ++i) extra(1 + i) = owned[static_cast<size_t>(i)].Index();
        if (Count() > 0) {
            for (int i = 0; i < 3; ++i)
                extra(4 + i) = static_cast<int32_t>(RelicFactory::PullNextRelicFromFront(Owner()));
        }
        // The six StringVars are presentation values. Their title formatting
        // does not change the relic draws or any simulated decision.
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList options;
        if (Count() >= 1)
            options.push_back(EventOption(SIM_OPTION(RelicTrader, Trade), "RELIC_TRADER.pages.INITIAL.options.TOP")
                                  .WithArg(0));
        if (Count() >= 2)
            options.push_back(EventOption(SIM_OPTION(RelicTrader, Trade), "RELIC_TRADER.pages.INITIAL.options.MIDDLE")
                                  .WithArg(1));
        if (Count() >= 3)
            options.push_back(EventOption(SIM_OPTION(RelicTrader, Trade), "RELIC_TRADER.pages.INITIAL.options.BOTTOM")
                                  .WithArg(2));
        if (options.empty())
            options.push_back(EventOption(SIM_OPTION(RelicTrader, Done), "PROCEED"));
        return options;
    }

    void Trade(const EventOptionState& option) const {
        int i = option.arg;
        if (i < 0 || i >= Count()) throw GameError("RelicTrader: invalid trade option");
        RelicCmd::Remove(Owned(i));
        RelicCmd::Obtain(New(i), Owner());
        Done(option);
    }

    void Done(const EventOptionState&) const {
        SetEventFinished(L10NLookup("RELIC_TRADER.pages.DONE.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { if (runState.CurrentActIndex() == 0) return false; return all_players(runState, [](Player p) { int n = 0; for (RelicModel r : p.Relics()) if (event_allowed::IsTradable(r)) ++n; return n >= 5; }); }
};

}  // namespace sts2
