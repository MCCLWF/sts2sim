#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"
#include <map>
#include <mutex>
#include "engine/sim/errors.h"
#include "game/All.h"

namespace sts2 {
namespace {
struct Catalog {
    std::mutex mutex;
    std::map<std::string, NativeCallbackRecord, std::less<>> records;
};
Catalog& catalog() { static Catalog value; return value; }
template<class Fn> std::string symbol_for(Fn fn) {
    if (!fn) return {};
    auto& c=catalog(); std::lock_guard lock(c.mutex);
    for (const auto& [name, r] : c.records) {
        if constexpr (std::is_same_v<Fn, MoveFn>) { if (r.move==fn) return name; }
        else if constexpr (std::is_same_v<Fn, CondFn>) { if (r.condition==fn) return name; }
        else if constexpr (std::is_same_v<Fn, WeightFn>) { if (r.weight==fn) return name; }
        else if constexpr (std::is_same_v<Fn, IntentDamageFn>) { if (r.intent_damage==fn) return name; }
        else if constexpr (std::is_same_v<Fn, IntentRepeatFn>) {if (r.intent_repeat==fn) return name;}
        else if constexpr (std::is_same_v<Fn, EventOptionFn>) {if (r.event_option==fn) return name;}
        else if (r.will_kill_player==fn) return name;
    }
    throw UnsupportedOperation("portable snapshot: native callback has no stable binding");
}
bool same_code(const NativeCallbackRecord& a, const NativeCallbackRecord& b) {
    return a.kind==b.kind && a.owner==b.owner && a.move==b.move && a.condition==b.condition
        && a.weight==b.weight && a.intent_damage==b.intent_damage && a.intent_repeat==b.intent_repeat
        && a.event_option==b.event_option && a.will_kill_player==b.will_kill_player;
}
}
void register_native_callback(NativeCallbackRecord record) {
    auto& c=catalog(); std::lock_guard lock(c.mutex);
    auto it=c.records.find(record.symbol);
    if (it!=c.records.end()) {
        if (!same_code(it->second,record)) throw GameError("native callback symbol collision: "+record.symbol);
    } else c.records.emplace(record.symbol,std::move(record));
}
NativeCallbackRecord native_callback(std::string_view symbol, NativeCallbackKind kind) {
    auto& c=catalog(); std::lock_guard lock(c.mutex);
    auto it=c.records.find(symbol);
    if (it==c.records.end() || it->second.kind!=kind)
        throw GameError("portable snapshot: unknown native callback symbol or type");
    return it->second;
}
std::vector<NativeCallbackRecord> native_callbacks() {
    auto& c=catalog(); std::lock_guard lock(c.mutex);
    std::vector<NativeCallbackRecord> records;
    for(const auto& [symbol,record]:c.records) records.push_back(record);
    return records;
}
std::string native_callback_symbol(MoveFn fn) { return symbol_for(fn); }
std::string native_callback_symbol(CondFn fn) { return symbol_for(fn); }
std::string native_callback_symbol(WeightFn fn) { return symbol_for(fn); }
std::string native_callback_symbol(IntentDamageFn fn) { return symbol_for(fn); }
std::string native_callback_symbol(IntentRepeatFn fn) { return symbol_for(fn); }
std::string native_callback_symbol(EventOptionFn fn) { return symbol_for(fn); }
std::string native_callback_symbol(WillKillPlayerFn fn) { return symbol_for(fn); }
}
