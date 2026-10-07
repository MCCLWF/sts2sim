// Models/EventModel.cs, Models/AncientEventModel.cs,
// Multiplayer/Game/EventSynchronizer.cs (ChooseOptionForEvent)
#include "game/Models/EventModel.h"

#include "core/xxhash64.h"
#include "engine/sim/alloc.h"
#include "game/All.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Hooks/HookGuard.h"

namespace sts2 {

namespace {

// AncientEventModel.RelicOption's OnChosen: obtain the relic, then Done().
void AncientRelicChosen(EventModel e, const EventOptionState& option) {
    RelicCmd::Obtain(RelicModel(&e.sim(), option.relic), e.Owner());
    e.st().custom_done_page = option.done_page;
    e.Done();
}

void InitVars(EventState& s) {
    const EventData& d = data(s.id);
    if (d.vars.size() > std::size(s.vars)) throw CapacityError("event has more vars than EventState holds");
    for (size_t i = 0; i < d.vars.size(); ++i) s.vars[i] = d.vars[i].base;
}

}  // namespace

EventModel EventModel::ToMutable(Sim* sim, EventId id) {
    EventState& s = sim->g->run.events[0];
    s = EventState{};
    s.id = id;
    InitVars(s);
    sim->g->run.event_synchronizer = EventSynchronizerState{
        .event_count = 1, .canonical_event = id, .has_player_vote_slot = true};
    return EventModel(sim, 1);
}

EventModel EventModel::Canonical(Sim* sim, EventId id) {
    EventState& s = sim->g->run.events[1];
    s = EventState{};
    s.id = id;
    InitVars(s);
    return AbstractModel::CanonicalView(EventModel(sim, 2),static_cast<uint16_t>(id));
}

sts2::Player EventModel::Owner() const { return st().began ? sts2::Player(sim_, 1) : sts2::Player{}; }

std::string EventModel::OptionKey(std::string_view pageName, std::string_view optionName) const {
    // $"{StringHelper.Slugify(GetType().Name)}.pages.{pageName}.options.{optionName}"
    std::string key(data().entry);
    key += ".pages.";
    key += pageName;
    key += ".options.";
    key += optionName;
    return key;
}

void EventModel::BeginEvent(sts2::Player player, bool isPreFinished) const {
    if (st().began) throw GameError("Tried to begin event, but it already has an owner!");
    st().began = true;
    // new Rng(Seed + (IsShared ? 0 : slotIndex) + hash(Id.Entry)); single player: slot 0.
    uint64_t seed = static_cast<uint64_t>(static_cast<int64_t>(player.RunState().Rng().seed) + 0) + deterministic_hash(data().entry);
    st().rng = sts2::Rng(seed);
    BeforeEventStarted(isPreFinished);
    CalculateVars();
    if (player.Creature().IsDead()) {
        SetEventFinished(L10NLookup("GENERIC.youAreDead.description"));
    } else {
        SetInitialEventState(isPreFinished);
    }
}

// EventModel.SetInitialEventState / AncientEventModel.SetInitialEventState
void EventModel::SetInitialEventState_Default(bool isPreFinished) const {
    if (IsAncient()) {
        EventOptionList readOnlyList = GenerateInitialOptionsWrapper();
        if (readOnlyList.empty() || isPreFinished) {
            StartPreFinished();
        } else {
            SetEventState(L10NLookup(std::string(data().entry) + ".pages.INITIAL.description"), readOnlyList);
        }
        return;
    }
    if (isPreFinished) throw GameError("Tried to load into pre-finished event! Only ancient events can be pre-finished.");
    EventOptionList eventOptions = GenerateInitialOptionsWrapper();
    SetEventState(L10NLookup(std::string(data().entry) + ".pages.INITIAL.description"), eventOptions);
}

// EventModel.GenerateInitialOptionsWrapper / AncientEventModel's (sealed).
// ReplaceNullOptions: options are values here; there is no null option.
EventOptionList EventModel::GenerateInitialOptionsWrapper_Default() const {
    if (IsAncient()) {
        // Hook.ShouldAllowAncient (AncientEventModel is not a hook parameter type yet).
        RequireNoRunOverride(sim_, Member::AbstractModel_ShouldAllowAncient, "ShouldAllowAncient");
        return GenerateInitialOptions();  // GeneratedOptions (DebugOption: never set)
    }
    return GenerateInitialOptions();
}

// AncientEventModel.BeforeEventStarted (EventModel's is empty).
void EventModel::BeforeEventStarted_Default(bool isPreFinished) const {
    if (!IsAncient() || isPreFinished) return;
    Creature creature = Owner().Creature();
    if (EventId_() == EventId::NEOW) creature.SetCurrentHpInternal(Dec(0));
    int32_t oldHp = creature.CurrentHp();
    Dec amount = Dec(creature.MaxHp() - creature.CurrentHp());
    if (sim_->g->run.has_ascension(AscensionLevel::WearyTraveler)) amount = amount * Dec::lit(8, 1);
    CreatureCmd::Heal(creature, amount, /*playAnim=*/false);
    st().healed_amount = creature.CurrentHp() - oldHp;
}

void EventModel::SetEventState_Default(LocKey description, const EventOptionList& eventOptions) const {
    EventState& s = st();
    EventOptionList copy = eventOptions;  // the list may be this event's own
    s.options = copy;
    s.description = description.hash;
    if (s.options.empty()) {
        if (s.is_finished) throw GameError("Tried to set event options after event was finished!");
        s.is_finished = true;
    }
    // StateChanged: EventRoom.OnEventStateChanged marks the room pre-finished (saves only).
}

void EventModel::SetEventFinished(LocKey description) const {
    SetEventState(description, EventOptionList{});
    st().is_finished = true;
    EnsureCleanup();
}

void EventModel::EnsureCleanup() const {
    if (!st().cleanup_called) {
        st().cleanup_called = true;
        OnEventFinished();
    }
}

bool EventModel::ChooseOption(int32_t optionIndex) const {
    if (IsFinished()) throw GameError("Option chosen for player, but the event is already finished!");
    if (optionIndex < 0 || optionIndex >= static_cast<int32_t>(st().options.size())) {
        throw GameError("attempted to choose an event option index out of range");
    }
    // NEventRoom.OptionButtonClicked: IsProceed calls option.Chosen directly;
    // only other options pass through EventSynchronizer's shared-page vote.
    if (IsShared() && !st().options[static_cast<size_t>(optionIndex)].is_proceed) {
        // ChooseOptionForSharedEvent clears votes, then increments the page
        // before invoking the single player's ChooseOptionForEvent.
        auto& sync = sim_->g->run.event_synchronizer;
        sync.has_player_vote = false;
        sync.player_vote = 0;
        ++sync.page_index;
    }
    EventOptionState& live = st().options[static_cast<size_t>(optionIndex)];
    EventOptionState option = live;
    // EventOption.Chosen
    if (option.on_chosen != 0 && (!option.disable_on_chosen || !option.was_chosen)) {
        live.was_chosen = true;  // WasChosen (BeforeChosen: UI subscribers only)
        option.was_chosen = true;
        registered_event_option_fn(option.on_chosen)(*this, option);
    }
    return option.is_proceed;
}

EventOption EventModel::RelicOption(RelicId relic, uint16_t onChosen, std::string_view pageName) const {
    return RelicOption(RelicModel(sim_, alloc_relic(*sim_->g, relic, 0)), onChosen, pageName);
}

EventOption EventModel::RelicOption(RelicModel relic, uint16_t onChosen, std::string_view pageName) const {
    relic.st().owner = Owner().Index();  // EventModel.RelicOption: relic.Owner = Owner
    EventOption option(onChosen, OptionKey(pageName, relic.data().entry));  // EventOption.FromRelic
    option.WithRelic(relic);
    return option;
}

EventOption EventModel::AncientRelicOption(RelicId relic, std::string_view pageName, std::string_view customDonePage) const {
    return AncientRelicOption(RelicModel(sim_, alloc_relic(*sim_->g, relic, 0)), pageName, customDonePage);
}

EventOption EventModel::AncientRelicOption(RelicModel relic, std::string_view pageName, std::string_view customDonePage) const {
    EventOption option = RelicOption(relic, register_event_option_fn(
        STS2_NATIVE_CALLBACK(EventOption, EventModel, (&AncientRelicChosen), "AncientRelicChosen")), pageName);
    if (!customDonePage.empty()) option.s.done_page = MakeLocKey(customDonePage).hash;
    return option;
}

void EventModel::StartPreFinished() const {
    std::string page = st().custom_done_page != 0 ? std::string(LocKey{st().custom_done_page}.str())
                                                   : std::string(data().entry) + ".pages.DONE.description";
    SetEventFinished(L10NLookup(page));
}

void EventModel::Done() const {
    // UpdateRunHistory: run history only.
    StartPreFinished();
}

}  // namespace sts2
