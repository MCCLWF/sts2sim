// Events/EventOption.cs
#include "game/Events/EventOption.h"

#include <mutex>
#include <vector>

#include "game/All.h"

namespace sts2 {

namespace {
template <class Fn>
struct Registry {
    std::mutex mu;
    std::vector<Fn> fns{nullptr};  // id 0 = null
    uint16_t add(Fn fn) {
        if(fn==nullptr) return 0;
        std::lock_guard<std::mutex> lock(mu);
        for (size_t i = 1; i < fns.size(); ++i) {
            if (fns[i] == fn) return static_cast<uint16_t>(i);
        }
        if(fns.size()>=0xFFFF) throw CapacityError("too many registered event callbacks");
        fns.push_back(fn);
        return static_cast<uint16_t>(fns.size() - 1);
    }
    Fn get(uint16_t id) {
        std::lock_guard<std::mutex> lock(mu);
        if (id >= fns.size()) throw GameError("unknown registered callback");
        return fns[id];
    }
};
Registry<EventOptionFn>& options() {
    static Registry<EventOptionFn> r;
    return r;
}
Registry<WillKillPlayerFn>& kills() {
    static Registry<WillKillPlayerFn> r;
    return r;
}
}  // namespace

uint16_t register_event_option_fn(EventOptionFn fn) { return options().add(fn); }
EventOptionFn registered_event_option_fn(uint16_t id) { return options().get(id); }
uint16_t register_will_kill_fn(WillKillPlayerFn fn) { return kills().add(fn); }
WillKillPlayerFn registered_will_kill_fn(uint16_t id) { return kills().get(id); }

EventOption& EventOption::WithRelic(RelicModel relic) {
    s.relic = relic.Index();
    return *this;
}

EventOption& EventOption::ThatDoesDamage(Dec damage) {
    s.will_kill_player = register_will_kill_fn(STS2_NATIVE_CALLBACK(WillKillPlayer, EventOption,
        ([](Player p, Dec value) { return Dec(p.Creature().CurrentHp()) <= value; }), "current_hp"));
    s.will_kill_value = damage;
    return *this;
}

EventOption& EventOption::ThatDecreasesMaxHp(Dec value) {
    s.will_kill_player = register_will_kill_fn(STS2_NATIVE_CALLBACK(WillKillPlayer, EventOption,
        ([](Player p, Dec v) { return Dec(p.Creature().MaxHp()) <= v; }), "max_hp"));
    s.will_kill_value = value;
    return *this;
}

bool WillKillPlayer(const EventOptionState& option, Player player) {
    if (option.will_kill_player == 0) return false;
    return registered_will_kill_fn(option.will_kill_player)(player, option.will_kill_value);
}

}  // namespace sts2
