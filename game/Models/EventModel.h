// Models/EventModel.cs and Models/AncientEventModel.cs. An event instance is
// an EventState in RunData.events: slot 1 is the room's event
// (EventSynchronizer's single-player event), slot 2 a scratch instance for
// calls on canonical events (IsAllowed). AncientEventModel is not a separate
// handle: its overrides of EventModel members are the `IsAncient()` branches
// of the defaults in EventModel.cpp. Concrete events: game/Models/Events/.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "content/generated/tables.h"
#include "game/Events/EventOption.h"
#include "game/Localization/DynamicVars/DynamicVarSet.h"
#include "game/Localization/LocKey.h"
#include "game/Models/AbstractModel.h"

namespace sts2 {

class EventModel : public AbstractModel {
public:
    EventModel() = default;
    explicit EventModel(Sim* sim, uint16_t slot = 1) : AbstractModel(sim, ModelKind::Event, slot) {}

    EventState& st() const { return sim_->g->run.events[index_ - 1]; }
    EventId EventId_() const { return st().id; }
    const EventData& data() const { return sts2::data(st().id); }
    bool IsAncient() const { return data().is_ancient; }
    bool IsShared() const { return data().is_shared; }

    // canonicalEvent.ToMutable() into the room's slot / a scratch canonical.
    static EventModel ToMutable(Sim* sim, EventId id);
    static EventModel Canonical(Sim* sim, EventId id);

    sts2::Player Owner() const;  // null before BeginEvent
    sts2::Rng& Rng() const { return st().rng; }
    bool IsFinished() const { return st().is_finished; }
    const EventOptionList& CurrentOptions() const { return st().options; }
    DynamicVarSet DynamicVars() const { return DynamicVarSet(*this); }

    // BeginEvent(player, combatSynchronizer, isPreFinished)
    void BeginEvent(sts2::Player player, bool isPreFinished) const;
    // EventSynchronizer.ChooseOptionForEvent + EventOption.Chosen. Returns true
    // when the chosen option has IsProceed (direct callback, not necessarily map).
    bool ChooseOption(int32_t optionIndex) const;
    void EnsureCleanup() const;

    // --- protected helpers of EventModel / AncientEventModel (for subclasses)
    void SetEventFinished(LocKey description) const;
    LocKey L10NLookup(std::string_view entryName) const { return MakeLocKey(entryName); }
    std::string OptionKey(std::string_view pageName, std::string_view optionName) const;
    std::string InitialOptionKey(std::string_view optionName) const { return OptionKey("INITIAL", optionName); }
    // EventModel.RelicOption(relic, onChosen, pageName): a new mutable relic owned by the player.
    EventOption RelicOption(RelicId relic, uint16_t onChosen, std::string_view pageName = "INITIAL") const;
    // EventModel.RelicOption(existing mutable relic, ...): used after a relic's
    // per-player SavedProperty setup; do not allocate a second instance.
    EventOption RelicOption(RelicModel relic, uint16_t onChosen, std::string_view pageName = "INITIAL") const;
    // AncientEventModel.RelicOption<T>(pageName, customDonePage): obtaining the relic ends the event.
    EventOption AncientRelicOption(RelicId relic, std::string_view pageName = "INITIAL",
                                   std::string_view customDonePage = {}) const;
    EventOption AncientRelicOption(RelicModel relic, std::string_view pageName = "INITIAL",
                                   std::string_view customDonePage = {}) const;
    void Done() const;              // AncientEventModel.Done
    void StartPreFinished() const;  // AncientEventModel.StartPreFinished

    // --- overridable members (EventModel.def)
#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const;                                          \
    Ret Name##_Default Params const;
#include "game/Models/EventModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM

protected:
    int32_t& extra(int i) const { return st().extra[i]; }
};

}  // namespace sts2
