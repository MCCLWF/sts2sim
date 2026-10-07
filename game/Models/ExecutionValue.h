// Read-only value freezing for privileged observations. No lazy getters.
#pragma once
#include <bit>
#include <type_traits>
#include "engine/json.h"
#include "engine/sim/execution_trace.h"
#include "game/Models/AbstractModel.h"
#include "game/Models/signature_types.h"

namespace sts2 {
struct MonsterSpawn;
struct MoveMachineDef;
struct LocKey;
class DynamicVar;
inline ExecutionValue UndefinedExecutionValue() {return {"{\"kind\":\"undefined\"}"};}
inline ExecutionValue VoidExecutionValue() {return {"{\"kind\":\"void\"}"};}
ExecutionValue FreezeExecutionValue(Dec);
ExecutionValue FreezeExecutionValue(AbstractModel);
ExecutionValue FreezeExecutionValue(Creature);
ExecutionValue FreezeExecutionValue(Player);
ExecutionValue FreezeExecutionValue(CombatState);
ExecutionValue FreezeExecutionValue(RunState);
ExecutionValue FreezeExecutionValue(const CardPlay*);
ExecutionValue FreezeExecutionValue(const DamageResult&);
ExecutionValue FreezeExecutionValue(const ResourceInfo&);
ExecutionValue FreezeExecutionValue(const AttackCommand&);
ExecutionValue FreezeExecutionValue(const DynamicVar&);
ExecutionValue FreezeExecutionValue(const PlayerChoiceContext&);
ExecutionValue FreezeExecutionValue(const AbstractRoom&);
ExecutionValue FreezeExecutionValue(const CombatRoom&);
ExecutionValue FreezeExecutionValue(const CardLocation&);
ExecutionValue FreezeExecutionValue(const CardCreationOptionsState&);
ExecutionValue FreezeExecutionValue(const EventOptionState&);
ExecutionValue FreezeExecutionValue(const ActMap&);
ExecutionValue FreezeExecutionValue(const MoveMachineDef&);
ExecutionValue FreezeExecutionValue(const MonsterSpawn&);
ExecutionValue FreezeExecutionValue(Reward);
ExecutionValue FreezeExecutionValue(const RewardList&);
ExecutionValue FreezeExecutionValue(const RewardsSet&);
ExecutionValue FreezeExecutionValue(LocKey);
ExecutionValue FreezeExecutionValue(CardKeywordSet);
ExecutionValue FreezeExecutionValue(RoomTypeSet);
ExecutionValue FreezeExecutionValue(Merchant::EntryRef);

template<class T> requires std::is_arithmetic_v<T>
ExecutionValue FreezeExecutionValue(T value) {
    JsonWriter w;w.begin_object();
    if constexpr(std::is_same_v<T,bool>) w.field("kind","bool").field("value",value);
    else if constexpr(std::is_same_v<T,float>) w.field("kind","f32").field("bits",uint64_t(std::bit_cast<uint32_t>(value)));
    else if constexpr(std::is_same_v<T,double>) w.field("kind","f64").field("bits",std::bit_cast<uint64_t>(value));
    else if constexpr(std::is_signed_v<T>) w.field("kind","integer").field("value",int64_t(value));
    else w.field("kind","integer").field("value",uint64_t(value));
    w.end_object();return {std::move(w.str())};
}
template<class T> requires std::is_enum_v<T>
ExecutionValue FreezeExecutionValue(T value) {
    JsonWriter w;w.begin_object().field("kind","enum").field("value",int64_t(value)).end_object();
    return {std::move(w.str())};
}
template<class T> requires requires(T value) {static_cast<AbstractModel>(value);}
ExecutionValue FreezeExecutionValue(T value) {return FreezeExecutionValue(static_cast<AbstractModel>(value));}
template<class T,size_t N>
ExecutionValue FreezeExecutionValue(const StaticVec<T,N>& values) {
    JsonWriter w;w.begin_object().field("kind","sequence").key("items").begin_array();bool complete=true;
    for(const auto& value:values) {auto frozen=FreezeExecutionValue(value);w.trusted_json(frozen.json);complete&=frozen.complete;}
    w.end_array().end_object();return {std::move(w.str()),complete};
}
}
