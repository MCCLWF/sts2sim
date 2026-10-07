#include "env/portable_registry.h"
#include "env_build_identity.h"
#include "engine/json.h"
#include "engine/sim/state.h"
#include "core/xxhash64.h"
#include "game/Localization/LocKey.h"
#include "content/generated/tables.h"
#include "game/All.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>
#include <cfenv>
#if defined(__GLIBC__)
#include <gnu/libc-version.h>
#endif
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace sts2::snapshot_detail {
namespace {
void check(bool condition, const char* reason) {
    if (!condition) throw GameError(std::string("portable snapshot: ")+reason);
}
void object(const Json& value, size_t count) {
    check(value.kind==Json::Object && value.object.size()==count,"missing or unexpected registry fields");
}
const std::vector<Json>& array(const Json& value, size_t capacity) {
    check(value.kind==Json::Array && value.array.size()<=capacity,"registry array capacity");
    return value.array;
}
std::string text(const Json& value) { check(value.kind==Json::String,"expected registry string"); return value.text; }
bool boolean(const Json& value) { check(value.kind==Json::Bool,"expected registry boolean"); return value.boolean; }
std::string cpp_owner(MonsterId id) {
    for (const auto& info:all_model_infos()) if (info.kind==ModelKind::Monster && info.index==static_cast<uint16_t>(id)) {
        std::string owner(info.type.substr(info.type.find_last_of('.')+1));
        if (owner=="Byrdpip" || owner=="PaelsLegion") owner+="Monster";
        return owner;
    }
    throw GameError("portable snapshot: unknown monster owner");
}
std::string cpp_event_owner(EventId id) {
    for(const auto& info:all_model_infos()) if(info.kind==ModelKind::Event && info.index==static_cast<uint16_t>(id))
        return std::string(info.type.substr(info.type.find_last_of('.')+1));
    throw GameError("portable snapshot: unknown event owner");
}
bool monster_owner_allowed(std::string_view owner,MonsterId id) {
    return owner==cpp_owner(id)
        || (owner=="FlailKnight" && id==MonsterId::MYSTERIOUS_KNIGHT)
        || (owner=="DecimillipedeSegmentLogic" && (id==MonsterId::DECIMILLIPEDE_SEGMENT_BACK
            || id==MonsterId::DECIMILLIPEDE_SEGMENT_FRONT || id==MonsterId::DECIMILLIPEDE_SEGMENT_MIDDLE));
}
template<class Fn> std::string symbol(Fn fn) { return native_callback_symbol(fn); }
void write_definition(JsonWriter& w, const MoveMachineDef& d) {
    w.begin_object().field("initial",int32_t(d.initial)).key("states").begin_array();
    for (const auto& s:d.states) {
        w.begin_object().field("kind",int32_t(s.kind)).field("id",s.id)
            .field("move",symbol(s.onPerform)).field("move_name",s.onPerformName)
            .field("follow_up",int32_t(s.followUp)).field("follow_up_id",s.followUpId)
            .field("must_perform_once",s.mustPerformOnceBeforeTransitioning).key("intents").begin_array();
        for (const auto& i:s.intents) w.begin_object().field("type",int32_t(i.type))
            .field("damage",i.damage).field("repeat",i.repeat).field("multi",i.multi)
            .field("damage_callback",symbol(i.damageCalc)).field("repeat_callback",symbol(i.repeatCalc))
            .field("strong",i.strong).field("count",i.count).end_object();
        w.end_array().key("weights").begin_array();
        for (const auto& b:s.weights) w.begin_object().field("state",int32_t(b.state))
            .field("repeat_type",int32_t(b.repeatType)).field("max_times",b.maxTimes)
            .field("callback",symbol(b.weightLambda)).field("f32_bits",uint64_t(std::bit_cast<uint32_t>(b.weight)))
            .field("cooldown",b.cooldown).end_object();
        w.end_array().key("branches").begin_array();
        for (const auto& b:s.branches) w.begin_object().field("state",int32_t(b.state))
            .field("callback",symbol(b.condition)).end_object();
        w.end_array().end_object();
    }
    w.end_array().end_object();
}
std::string definition_key(const MoveMachineDef& d) { JsonWriter w; write_definition(w,d); return w.str(); }
NativeCallbackRecord callback(const Json& j, NativeCallbackKind kind, const std::vector<MonsterId>& owners) {
    const std::string name=text(j);
    if (name.empty()) return {.kind=kind};
    auto record=native_callback(name,kind);
    for (auto owner:owners) check(monster_owner_allowed(record.owner,owner),"callback belongs to a different monster model");
    return record;
}
MoveMachineDef read_definition(const Json& j, const std::vector<MonsterId>& owners) {
    object(j,2); MoveMachineDef d; d.initial=j.at("initial").integer<int8_t>();
    const auto& states=array(j.at("states"),kMaxMonsterStates); check(!states.empty(),"empty machine definition");
    std::set<std::string> names;
    auto index=[&](int v) { check(v>=-1 && (v==-1 || size_t(v)<states.size()),"machine definition state index"); };
    index(d.initial); check(d.initial>=0,"machine initial state missing");
    for (const auto& value:states) {
        object(value,10); MonsterStateDef s;
        const int kind=value.at("kind").integer<uint8_t>(); check(kind<=2,"unknown machine state kind");
        s.kind=static_cast<MonsterStateKind>(kind); s.id=text(value.at("id"));
        check(!s.id.empty() && names.insert(s.id).second,"empty or duplicate machine state ID");
        s.onPerform=callback(value.at("move"),NativeCallbackKind::Move,owners).move;
        s.onPerformName=text(value.at("move_name")); s.followUp=value.at("follow_up").integer<int8_t>(); index(s.followUp);
        s.followUpId=text(value.at("follow_up_id"));
        s.mustPerformOnceBeforeTransitioning=boolean(value.at("must_perform_once"));
        for (const auto& iv:array(value.at("intents"),4)) {
            object(iv,8); Intent i; auto type=iv.at("type").integer<uint8_t>();
            check(!name(static_cast<IntentType>(type)).empty(),"unknown intent type"); i.type=static_cast<IntentType>(type);
            i.damage=iv.at("damage").integer<int32_t>(); i.repeat=iv.at("repeat").integer<int32_t>(); i.multi=boolean(iv.at("multi"));
            i.damageCalc=callback(iv.at("damage_callback"),NativeCallbackKind::IntentDamage,owners).intent_damage;
            i.repeatCalc=callback(iv.at("repeat_callback"),NativeCallbackKind::IntentRepeat,owners).intent_repeat;
            i.strong=boolean(iv.at("strong")); i.count=iv.at("count").integer<int32_t>(); s.intents.push_back(i);
        }
        for (const auto& bv:array(value.at("weights"),64)) {
            object(bv,6); StateWeight b; b.state=bv.at("state").integer<int8_t>(); index(b.state);
            auto repeat=bv.at("repeat_type").integer<uint8_t>();
            check(!name(static_cast<MoveRepeatType>(repeat)).empty(),"unknown move repeat type");
            b.repeatType=static_cast<MoveRepeatType>(repeat); b.maxTimes=bv.at("max_times").integer<int32_t>();
            b.weightLambda=callback(bv.at("callback"),NativeCallbackKind::Weight,owners).weight;
            b.weight=std::bit_cast<float>(bv.at("f32_bits").integer<uint32_t>()); check(std::isfinite(b.weight),"nonfinite machine weight");
            b.cooldown=bv.at("cooldown").integer<int32_t>(); s.weights.push_back(b);
        }
        for (const auto& bv:array(value.at("branches"),64)) {
            object(bv,2); ConditionalBranch b; b.state=bv.at("state").integer<int8_t>(); index(b.state);
            b.condition=callback(bv.at("callback"),NativeCallbackKind::Condition,owners).condition; s.branches.push_back(b);
        }
        check(s.kind==MonsterStateKind::Move || (!s.onPerform && s.intents.empty() && s.onPerformName.empty()),"non-move state has move effect");
        check(s.kind==MonsterStateKind::RandomBranch || s.weights.empty(),"weights outside random branch");
        check(s.kind==MonsterStateKind::ConditionalBranch || s.branches.empty(),"conditions outside conditional branch");
        d.states.push_back(std::move(s));
    }
    return d;
}
template<class F> void each_state(const GameState& state, const GameState* base, F&& f) {
    f(state); if (base) f(*base);
}
template<class F> void each_monster(const GameState& state, F&& f) {
    state.creatures.visit_snapshot([](auto,auto){},[&](uint32_t,const CreatureState& c){ f(c.monster); });
}
template<class F> void each_loc_key(const GameState& state,F&& f) {
    for (const auto& event:state.run.events) {
        f(event.description); f(event.custom_done_page);
        for (const auto& option:event.options) { f(option.text_key);f(option.done_page); }
    }
}
bool stun_owner_allowed(std::string_view owner,MonsterId monster) {
    return monster_owner_allowed(owner,monster)
        || (owner=="FlutterPower" && monster==MonsterId::THIEVING_HOPPER)
        || (owner=="RavenousPower" && monster==MonsterId::CORPSE_SLUG);
}
bool event_owner_allowed(const NativeCallbackRecord& callback,EventId id) {
    if(id==EventId::NONE) return false;
    if(callback.kind==NativeCallbackKind::EventOption)
        return callback.owner==cpp_event_owner(id) || (callback.owner=="EventModel" && data(id).is_ancient);
    return callback.owner=="EventOption" || callback.owner==cpp_event_owner(id);
}
}

PortableRegistry collect_portable_registry(const GameState& state,const GameState* base) {
    PortableRegistry r;
    std::map<std::string,PortableMachine> definitions;
    std::map<uint16_t,std::string> def_keys,stun_keys;
    std::map<uint16_t,std::string> event_keys,kill_keys;
    each_state(state,base,[&](const GameState& g) {
        each_monster(g,[&](const MonsterState& m) {
            if (m.machine.definition) {
                check(move_machine_matches_model(m.machine.definition,m.id),"machine owner binding");
                auto d=move_machine_def(m.machine.definition); auto key=definition_key(d); def_keys[m.machine.definition]=key;
                auto& entry=definitions[key]; entry.definition=std::move(d);
                if (std::find(entry.owners.begin(),entry.owners.end(),m.id)==entry.owners.end()) entry.owners.push_back(m.id);
            }
            if (m.next_move.stun_move) {
                auto fn=registered_move_fn(m.next_move.stun_move); check(fn!=nullptr,"missing stun callback");
                auto key=symbol(fn); check(stun_owner_allowed(native_callback(key,NativeCallbackKind::Move).owner,m.id),"stun callback owner");
                stun_keys[m.next_move.stun_move]=std::move(key);
            }
        });
        for(const auto& event:g.run.events) for(const auto& option:event.options) {
            if(option.on_chosen) {
                auto key=symbol(registered_event_option_fn(option.on_chosen));
                check(!key.empty() && event_owner_allowed(native_callback(key,NativeCallbackKind::EventOption),event.id),"event callback owner");
                event_keys[option.on_chosen]=std::move(key);
            }
            if(option.will_kill_player) {
                auto key=symbol(registered_will_kill_fn(option.will_kill_player));
                check(!key.empty() && event_owner_allowed(native_callback(key,NativeCallbackKind::WillKillPlayer),event.id),"event predicate owner");
                kill_keys[option.will_kill_player]=std::move(key);
            }
        }
        each_loc_key(g,[&](uint32_t key) { if(key) {
            auto value=std::string(LocKey{key}.str()); check(!value.empty(),"localization identity is unavailable"); r.localization.emplace(key,std::move(value));
        }});
    });
    std::map<std::string,uint16_t> def_ids,stun_ids;
    for (auto& [key,value]:definitions) {
        std::sort(value.owners.begin(),value.owners.end());
        r.machines.push_back(std::move(value)); def_ids[key]=static_cast<uint16_t>(r.machines.size());
    }
    std::set<std::string> stuns; for (const auto& [id,key]:stun_keys) stuns.insert(key);
    for (const auto& key:stuns) { r.stuns.push_back(key); stun_ids[key]=static_cast<uint16_t>(r.stuns.size()); }
    for (const auto& [id,key]:def_keys) r.machine_ids[id]=def_ids.at(key);
    for (const auto& [id,key]:stun_keys) r.stun_ids[id]=stun_ids.at(key);
    auto canonical_callbacks=[](const auto& keys,auto& names,auto& mapping) {
        std::set<std::string> sorted;for(const auto& [id,key]:keys) sorted.insert(key);
        std::map<std::string,uint16_t> indices;
        for(const auto& key:sorted) {names.push_back(key);indices[key]=static_cast<uint16_t>(names.size());}
        for(const auto& [id,key]:keys) mapping[id]=indices.at(key);
    };
    canonical_callbacks(event_keys,r.event_options,r.event_option_ids);
    canonical_callbacks(kill_keys,r.will_kill,r.will_kill_ids);
    return r;
}
void remap_portable_state(GameState& state,const PortableRegistry& r) {
    for (uint32_t ref=1;ref<=state.creatures.capacity();++ref) if (state.creatures.live(ref)) {
        auto& m=state.creatures[ref].monster;
        if (m.machine.definition) { auto it=r.machine_ids.find(m.machine.definition); check(it!=r.machine_ids.end(),"unbound machine reference"); m.machine.definition=it->second; }
        if (m.next_move.stun_move) { auto it=r.stun_ids.find(m.next_move.stun_move); check(it!=r.stun_ids.end(),"unbound stun reference"); m.next_move.stun_move=it->second; }
    }
    for(auto& event:state.run.events) for(auto& option:event.options) {
        auto remap=[](uint16_t& ref,const auto& mapping) {
            if(ref) {auto it=mapping.find(ref);check(it!=mapping.end(),"unbound event callback reference");ref=it->second;}
        };
        remap(option.on_chosen,r.event_option_ids);remap(option.will_kill_player,r.will_kill_ids);
    }
}
void validate_portable_registry_usage(const GameState& state,const GameState* base,const PortableRegistry& r) {
    std::vector<std::set<MonsterId>> used(r.machines.size()); std::set<uint16_t> stuns; std::set<uint32_t> loc;
    std::set<uint16_t> options,kills;
    each_state(state,base,[&](const GameState& g) {
        each_monster(g,[&](const MonsterState& m) {
            if (m.machine.definition) {
                check(m.machine.definition<=r.machines.size(),"portable machine reference outside registry");
                used[m.machine.definition-1].insert(m.id);
            }
            if (m.next_move.stun_move) {
                check(m.next_move.stun_move<=r.stuns.size(),"portable stun reference outside registry");
                check(stun_owner_allowed(native_callback(r.stuns[m.next_move.stun_move-1],NativeCallbackKind::Move).owner,m.id),"stun callback owner");
                stuns.insert(m.next_move.stun_move);
            }
        });
        for(const auto& event:g.run.events) for(const auto& option:event.options) {
            if(option.on_chosen) {
                check(option.on_chosen<=r.event_options.size(),"portable event reference outside registry");
                check(event_owner_allowed(native_callback(r.event_options[option.on_chosen-1],NativeCallbackKind::EventOption),event.id),"event callback owner");
                options.insert(option.on_chosen);
            }
            if(option.will_kill_player) {
                check(option.will_kill_player<=r.will_kill.size(),"portable predicate reference outside registry");
                check(event_owner_allowed(native_callback(r.will_kill[option.will_kill_player-1],NativeCallbackKind::WillKillPlayer),event.id),"event predicate owner");
                kills.insert(option.will_kill_player);
            }
        }
        each_loc_key(g,[&](uint32_t key){if(key) loc.insert(key);});
    });
    for(size_t i=0;i<used.size();++i) check(used[i]==std::set<MonsterId>(r.machines[i].owners.begin(),r.machines[i].owners.end()),"unused or incorrect machine owners");
    check(stuns.size()==r.stuns.size(),"unused stun bindings");
    check(options.size()==r.event_options.size() && kills.size()==r.will_kill.size(),"unused event bindings");
    check(loc.size()==r.localization.size(),"incomplete or unused localization dictionary");
    for(auto key:loc) check(r.localization.contains(key),"missing localization identity");
}
void write_portable_registry(JsonWriter& w,const PortableRegistry& r) {
    w.begin_object().key("machines").begin_array();
    for(const auto& m:r.machines) {
        w.begin_object().key("owners").begin_array(); for(auto owner:m.owners) w.value(int32_t(owner));
        w.end_array().key("definition"); write_definition(w,m.definition); w.end_object();
    }
    w.end_array().key("stuns").begin_array(); for(const auto& s:r.stuns) w.value(s);
    w.end_array().key("event_options").begin_array();for(const auto& s:r.event_options) w.value(s);
    w.end_array().key("will_kill").begin_array();for(const auto& s:r.will_kill) w.value(s);
    w.end_array().key("localization").begin_array();
    for(const auto& [hash,key]:r.localization) w.begin_object().field("hash",uint64_t(hash)).field("key",key).end_object();
    w.end_array().end_object();
}
PortableRegistry read_portable_registry(const Json& j) {
    object(j,5); PortableRegistry r; std::string last;
    for(const auto& value:array(j.at("machines"),65535)) {
        object(value,2); PortableMachine m; uint16_t previous=0;
        for(const auto& owner:array(value.at("owners"),static_cast<size_t>(MonsterId::Count))) {
            auto id=owner.integer<uint16_t>(); check(id>previous && id<static_cast<uint16_t>(MonsterId::Count),"invalid machine owners");
            previous=id; m.owners.push_back(static_cast<MonsterId>(id));
        }
        check(!m.owners.empty(),"machine has no owner"); m.definition=read_definition(value.at("definition"),m.owners);
        auto key=definition_key(m.definition); check(last.empty() || last<key,"noncanonical or duplicate machine definitions"); last=std::move(key);
        r.machines.push_back(std::move(m));
    }
    last.clear();
    for(const auto& value:array(j.at("stuns"),65535)) {
        auto key=text(value); check(!key.empty() && (last.empty() || last<key),"noncanonical or duplicate stun bindings");
        native_callback(key,NativeCallbackKind::Move); last=key; r.stuns.push_back(std::move(key));
    }
    auto read_callbacks=[&](const char* field,NativeCallbackKind kind,auto& names) {
        std::string previous;
        for(const auto& value:array(j.at(field),48)) {
            auto key=text(value);check(!key.empty() && (previous.empty() || previous<key),"noncanonical or duplicate event bindings");
            native_callback(key,kind);previous=key;names.push_back(std::move(key));
        }
    };
    read_callbacks("event_options",NativeCallbackKind::EventOption,r.event_options);
    read_callbacks("will_kill",NativeCallbackKind::WillKillPlayer,r.will_kill);
    uint32_t previous=0;
    for(const auto& value:array(j.at("localization"),4096)) {
        object(value,2); auto hash=value.at("hash").integer<uint32_t>(); auto key=text(value.at("key"));
        check(hash>previous && !key.empty(),"invalid localization dictionary order"); previous=hash;
        // Verify FNV without publishing into the process table until usage and
        // wire integrity have passed.
        uint32_t expected=2166136261u; for(unsigned char c:key) expected=(expected^c)*16777619u; if(!expected) expected=1;
        check(hash==expected,"localization key hash mismatch"); r.localization.emplace(hash,std::move(key));
    }
    return r;
}
void install_portable_registry(PortableRegistry& r) {
    for(size_t i=0;i<r.machines.size();++i) {
        const auto& m=r.machines[i]; uint16_t id=0;
        for(auto owner:m.owners) id=intern_move_machine(m.definition,owner);
        r.machine_ids[static_cast<uint16_t>(i+1)]=id;
    }
    for(size_t i=0;i<r.stuns.size();++i) r.stun_ids[static_cast<uint16_t>(i+1)]=register_move_fn(native_callback(r.stuns[i],NativeCallbackKind::Move).move);
    for(size_t i=0;i<r.event_options.size();++i) r.event_option_ids[static_cast<uint16_t>(i+1)]=register_event_option_fn(native_callback(r.event_options[i],NativeCallbackKind::EventOption).event_option);
    for(size_t i=0;i<r.will_kill.size();++i) r.will_kill_ids[static_cast<uint16_t>(i+1)]=register_will_kill_fn(native_callback(r.will_kill[i],NativeCallbackKind::WillKillPlayer).will_kill_player);
    for(const auto& [hash,key]:r.localization) check(MakeLocKey(key).hash==hash,"localization collision");
}
void write_snapshot_json(JsonWriter& w,const Json& j) {
    switch(j.kind) {
    case Json::Null: w.null(); break;
    case Json::Bool: w.value(j.boolean); break;
    case Json::String: w.value(j.text); break;
    case Json::Number: if(j.text.starts_with('-')) w.value(j.integer<int64_t>()); else w.value(j.integer<uint64_t>()); break;
    case Json::Array: w.begin_array(); for(const auto& child:j.array) write_snapshot_json(w,child); w.end_array(); break;
    case Json::Object: w.begin_object(); for(const auto& [key,child]:j.object) {w.key(key);write_snapshot_json(w,child);} w.end_object(); break;
    }
}
std::string portable_payload(const Json& state,const Json& registry) {
    JsonWriter w; w.begin_object().field("format","sts2env.portable_snapshot").field("version",1)
        .field("native_build_sha256",kNativeBuildSha256).field("native_runtime_abi",native_runtime_abi())
        .key("registry");write_snapshot_json(w,registry);
    w.key("logical_state");write_snapshot_json(w,state);w.end_object();return w.str();
}
uint64_t portable_wire_hash(std::string_view payload) { return xxhash64(payload.data(),payload.size(),0x73747332706f7274ULL); }
std::string native_runtime_abi() {
    std::string result="round="+std::to_string(std::fegetround());
#if defined(__GLIBC__)
    // Gaussian and other native math operations use the host's math runtime.
    result+=";glibc=";result+=gnu_get_libc_version();
#endif
#if defined(__SSE__)
    // Include rounding, denormal handling and exception masks, excluding
    // accumulated exception status bits which do not change future results.
    result+=";mxcsr_control="+std::to_string(_mm_getcsr()&0xffc0U);
#endif
    return result;
}
}
