// Events/EventOption.cs. An option is a value (EventOptionState): the C#
// OnChosen closure is a registered callback plus what it captured (`arg`,
// `relic`). Callbacks receive the event and the option:
//   EventOption(*this, SIM_OPTION(Neow, OnSomething), InitialOptionKey("X"))
#pragma once

#include <cstdint>
#include <string_view>

#include "engine/sim/state.h"
#include "game/fwd.h"
#include "game/Localization/LocKey.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/NativeCallback.h"

namespace sts2 {

class EventModel;


// Process-wide registries of the callbacks (ids are stored in state; 0 = null).
uint16_t register_event_option_fn(EventOptionFn fn);
EventOptionFn registered_event_option_fn(uint16_t id);
uint16_t register_will_kill_fn(WillKillPlayerFn fn);
WillKillPlayerFn registered_will_kill_fn(uint16_t id);

#define SIM_OPTION(Cls, Method)                                                                     \
    ::sts2::register_event_option_fn(STS2_NATIVE_CALLBACK(EventOption, Cls, ([](::sts2::EventModel e, const ::sts2::EventOptionState& o) { \
        Cls(e).Method(o);                                                                           \
    }), #Method))

using EventOptionList = StaticVec<EventOptionState, 24>;

// The EventOption constructors and builder methods, on the value.
struct EventOption {
    EventOptionState s;

    // new EventOption(eventModel, onChosen, textKey[, hoverTips][, isProceed, disableOnChosen]).
    // onChosen 0 = null: the option is locked.
    EventOption(uint16_t onChosen, std::string_view textKey, bool isProceed = false, bool disableOnChosen = true) {
        s.text_key = MakeLocKey(textKey).hash;
        s.on_chosen = onChosen;
        s.is_locked = onChosen == 0;
        s.is_proceed = isProceed;
        s.disable_on_chosen = disableOnChosen;
    }
    EventOption& WithRelic(RelicModel relic);
    EventOption& WithArg(int32_t arg) {
        s.arg = arg;
        return *this;
    }
    EventOption& ThatDoesDamage(Dec damage);       // WillKillPlayer: CurrentHp <= damage
    EventOption& ThatDecreasesMaxHp(Dec value);    // WillKillPlayer: MaxHp <= value
    EventOption& ThatWontSaveToChoiceHistory() { return *this; }  // run history only
    EventOption& ThatHasDynamicTitle() { return *this; }          // run history only
    operator EventOptionState() const { return s; }
};

// Whether the option would kill `player` (the C# WillKillPlayer delegate).
bool WillKillPlayer(const EventOptionState& option, Player player);

}  // namespace sts2
