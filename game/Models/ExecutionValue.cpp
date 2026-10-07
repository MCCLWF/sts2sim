#include "game/Models/ExecutionValue.h"
#include "game/All.h"
#include "game/Commands/Builders/AttackCommand.h"
#include "game/Events/EventOption.h"
#include "game/Models/EncounterModel.h"
#include "engine/sim/model_info.h"
#include "engine/sim/state_fields.generated.h"

namespace sts2 {
namespace {
struct Encoder {
    JsonWriter w;bool complete=true;
    explicit Encoder(std::string_view kind) {w.begin_object().field("kind",kind);}
    template<class T> void field(std::string_view name,const T& value) {
        auto frozen=FreezeExecutionValue(value);complete&=frozen.complete;w.key(name).trusted_json(frozen.json);
    }
    ExecutionValue finish() {w.end_object();return {std::move(w.str()),complete};}
};
// Complete logical POD fields, excluding inactive array/vector capacity.
struct PodEncoder {
    JsonWriter& w;
    template<class T> void value(const T& value) {
        if constexpr(std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T,Dec>) {
            w.trusted_json(FreezeExecutionValue(value).json);
        } else {
            w.begin_object();visit_named_state_fields(value,[&](const char* name,const auto& field) {
                w.key(name);this->value(field);
            });w.end_object();
        }
    }
    template<class T,size_t N> void value(const T (&values)[N]) {sequence(values);}
    template<class T,size_t N> void value(const StaticVec<T,N>& values) {sequence(values);}
    template<class T,size_t N> void value(const DotnetSet<T,N>& values) {
        // Set value order is the engine's original .NET enumeration order.
        w.begin_array();values.for_each([&](const T& v){value(v);});w.end_array();
    }
    template<class T> void sequence(const T& values) {w.begin_array();for(const auto& v:values)value(v);w.end_array();}
};
template<class T> ExecutionValue pod(const T& value) {
    JsonWriter w;w.begin_object().field("kind","logical_value").key("fields");PodEncoder{w}.value(value);w.end_object();
    return {std::move(w.str())};
}
template<class Fn> void callback(Encoder& encoder,std::string_view name,Fn fn) {
    encoder.w.key(name);
    try {encoder.w.value(native_callback_symbol(fn));}
    catch(const UnsupportedOperation&) {
        encoder.w.begin_object().field("unclassified",true).end_object();encoder.complete=false;
    }
}
ExecutionValue null_ref(std::string_view kind) {JsonWriter w;w.begin_object().field("kind",kind).field("null",true).end_object();return {std::move(w.str())};}
}
ExecutionValue FreezeExecutionValue(Dec value) {
    JsonWriter w;w.begin_object().field("kind","decimal").key("bits").begin_array()
        .value(uint64_t(value.lo)).value(uint64_t(value.mid)).value(uint64_t(value.hi)).value(uint64_t(value.flags)).end_array().end_object();
    return {std::move(w.str())};
}
ExecutionValue FreezeExecutionValue(AbstractModel model) {
    if(!model)return null_ref("model_ref");
    bool complete=model.Kind()<=ModelKind::Orb;
    JsonWriter w;w.begin_object().field("kind","model_ref").field("ref",uint64_t(model.IsCanonical()?0:model.Index()))
        .field("model_kind",int32_t(model.Kind())).key("is_canonical");
    if(complete)w.value(model.IsCanonical());else w.null();
    const ModelInfo* info=complete?model_info(model.Kind(),model.DenseId()):nullptr;
    w.field("model_id",info?info->id:std::string_view{}).end_object();
    return {std::move(w.str()),complete && info};
}
ExecutionValue FreezeExecutionValue(Creature value) {
    if(!value)return null_ref("creature_ref");
    JsonWriter w;w.begin_object().field("kind","creature_ref").field("ref",uint64_t(value.Index()))
        .field("combat_id",value.st().combat_id).end_object();return {std::move(w.str())};
}
ExecutionValue FreezeExecutionValue(Player value) {
    if(!value)return null_ref("player_ref");
    JsonWriter w;w.begin_object().field("kind","player_ref").field("ref",uint64_t(value.Index()))
        .field("net_id",value.st().net_id).end_object();return {std::move(w.str())};
}
ExecutionValue FreezeExecutionValue(CombatState value) {
    JsonWriter w;w.begin_object().field("kind","combat_ref").field("bound",value.sim_!=nullptr)
        .field("exists",value.sim_ && value.sim_->g->combat.exists).end_object();return {std::move(w.str())};
}
ExecutionValue FreezeExecutionValue(RunState value) {
    JsonWriter w;w.begin_object().field("kind","run_ref").field("bound",bool(value)).end_object();return {std::move(w.str())};
}
ExecutionValue FreezeExecutionValue(const ResourceInfo& value) {
    Encoder w("resource_info");w.field("energy_spent",value.EnergySpent);w.field("energy_value",value.EnergyValue);
    w.field("stars_spent",value.StarsSpent);w.field("star_value",value.StarValue);return w.finish();
}
ExecutionValue FreezeExecutionValue(const CardPlay* value) {
    if(!value)return null_ref("card_play");
    Encoder w("card_play");w.field("card",value->Card);w.field("player",value->Player);w.field("target",value->Target);
    w.field("result_pile",value->ResultPile);w.field("resources",value->Resources);w.field("auto_play",value->IsAutoPlay);
    w.field("play_index",value->PlayIndex);w.field("play_count",value->PlayCount);
    w.field("block_history_count",value->BlockHistoryCount);w.field("block_history_round",value->BlockHistoryRound);
    w.field("block_history_player_turn",value->BlockHistoryPlayerTurn);w.field("block_history_side",value->BlockHistorySide);
    w.field("block_history_epoch",value->BlockHistoryEpoch);return w.finish();
}
ExecutionValue FreezeExecutionValue(const DamageResult& value) {
    Encoder w("damage_result");w.field("receiver",value.Receiver);w.field("props",value.Props);
    w.field("blocked",value.BlockedDamage);w.field("unblocked",value.UnblockedDamage);w.field("overkill",value.OverkillDamage);
    w.field("block_broken",value.WasBlockBroken);w.field("fully_blocked",value.WasFullyBlocked);w.field("target_killed",value.WasTargetKilled);
    return w.finish();
}
ExecutionValue FreezeExecutionValue(const DynamicVar& value) {
    Encoder w("dynamic_var_ref");w.field("owner",value.owner_);w.field("index",value.index_);return w.finish();
}
ExecutionValue FreezeExecutionValue(const AttackCommand& value) {
    Encoder w("attack_command");w.field("damage_per_hit",value.damagePerHit_);w.field("damage_var",value.calculatedDamageVar_);
    w.field("hit_count",value.hitCount_);w.field("source_type",value.sourceType_);w.field("combat",value.combatState_);
    w.field("single_target",value.singleTarget_);w.field("random_allows_duplicates",value.doesRandomTargetingAllowDuplicates_);
    w.field("attacker",value.attacker_);w.field("model_source",value.modelSource_);w.field("card_play",value.cardPlay_);
    w.field("target_side",value.targetSide_);w.field("damage_props",value.damageProps_);w.field("random_targeting",value.isRandomlyTargeted_);
    w.field("gigantification_capture",value.gigantificationCapture_);w.field("play_on_every_hit",value.playOnEveryHit_);
    w.w.key("results").begin_array();for(const auto& group:value.results_) {
        w.w.begin_array();for(const auto& result:group)w.w.trusted_json(FreezeExecutionValue(result).json);w.w.end_array();
    }w.w.end_array();
    w.w.field("before_damage_callback_present",bool(value.beforeDamage_)).field("after_animation_callback_present",bool(value.afterAttackerAnim_));
    if(value.beforeDamage_ || value.afterAttackerAnim_)w.complete=false; // no fabricated delegate identity
    return w.finish();
}
ExecutionValue FreezeExecutionValue(const PlayerChoiceContext& value) {
    Encoder w("choice_context");w.field("throwing",value.throwing_);w.field("models",value.models_);return w.finish();
}
ExecutionValue FreezeExecutionValue(const AbstractRoom& value) {
    if(!value)return null_ref("room_ref");
    Encoder w("room_ref");w.field("room_type",value.RoomType());w.field("exited_encounter",value.ExitedEncounter());
    w.field("ran_out_of_time",value.EncounterRanOutOfTime());w.field("extra_rewards",value.ExtraRewards());return w.finish();
}
ExecutionValue FreezeExecutionValue(const CombatRoom& value) {return FreezeExecutionValue(static_cast<const AbstractRoom&>(value));}
ExecutionValue FreezeExecutionValue(const CardLocation& value) {
    Encoder w("card_location");w.field("player",value.player);w.field("pile_type",value.pileType);w.field("position",value.position);return w.finish();
}
ExecutionValue FreezeExecutionValue(const CardCreationOptionsState& value) {return pod(value);}
ExecutionValue FreezeExecutionValue(const ActMap& value) {return pod(value);}
ExecutionValue FreezeExecutionValue(const EventOptionState& value) {
    Encoder w("event_option");w.field("text",LocKey{value.text_key});w.field("locked",value.is_locked);w.field("proceed",value.is_proceed);
    w.field("chosen",value.was_chosen);w.field("disable_on_chosen",value.disable_on_chosen);w.field("relic_ref",value.relic);
    w.field("arg",value.arg);w.field("done_page",LocKey{value.done_page});w.field("will_kill_value",value.will_kill_value);
    callback(w,"on_chosen",registered_event_option_fn(value.on_chosen));
    callback(w,"will_kill",registered_will_kill_fn(value.will_kill_player));return w.finish();
}
ExecutionValue FreezeExecutionValue(const MoveMachineDef& value) {
    Encoder w("move_machine");w.field("initial",value.initial);w.w.key("states").begin_array();
    for(const auto& state:value.states) {
        Encoder s("move_state");s.w.field("id",state.id).field("move_name",state.onPerformName);
        s.field("state_kind",state.kind);s.field("follow_up",state.followUp);s.w.field("follow_up_id",state.followUpId);
        s.field("must_perform_once",state.mustPerformOnceBeforeTransitioning);callback(s,"move",state.onPerform);
        s.w.key("intents").begin_array();for(const auto& intent:state.intents) {
            Encoder i("intent");i.field("intent_type",intent.type);i.field("damage",intent.damage);i.field("repeat",intent.repeat);
            i.field("multi",intent.multi);i.field("strong",intent.strong);i.field("count",intent.count);
            callback(i,"damage_callback",intent.damageCalc);callback(i,"repeat_callback",intent.repeatCalc);
            auto frozen=i.finish();s.complete&=frozen.complete;s.w.trusted_json(frozen.json);
        }s.w.end_array().key("weights").begin_array();for(const auto& weight:state.weights) {
            Encoder b("weight");b.field("state",weight.state);b.field("repeat_type",weight.repeatType);b.field("max_times",weight.maxTimes);
            b.field("weight",weight.weight);b.field("cooldown",weight.cooldown);callback(b,"callback",weight.weightLambda);
            auto frozen=b.finish();s.complete&=frozen.complete;s.w.trusted_json(frozen.json);
        }s.w.end_array().key("branches").begin_array();for(const auto& branch:state.branches) {
            Encoder b("branch");b.field("state",branch.state);callback(b,"condition",branch.condition);
            auto frozen=b.finish();s.complete&=frozen.complete;s.w.trusted_json(frozen.json);
        }s.w.end_array();auto frozen=s.finish();w.complete&=frozen.complete;w.w.trusted_json(frozen.json);
    }w.w.end_array();return w.finish();
}
ExecutionValue FreezeExecutionValue(const MonsterSpawn& value) {Encoder w("monster_spawn");w.field("monster",value.monster);w.field("slot",value.slot);return w.finish();}
ExecutionValue FreezeExecutionValue(Reward value) {
    if(!value)return null_ref("reward_ref");
    JsonWriter w;w.begin_object().field("kind","reward_ref").field("ref",uint64_t(value.Index()))
        .field("reward_type",int32_t(value.st().type)).end_object();return {std::move(w.str())};
}
ExecutionValue FreezeExecutionValue(const RewardList& value) {
    if(!value.sim_)return null_ref("reward_list");
    Encoder w("reward_list");w.w.key("items").begin_array();for(size_t i=0;i<value.size();i++)w.w.trusted_json(FreezeExecutionValue(value[i]).json);
    w.w.end_array();return w.finish();
}
ExecutionValue FreezeExecutionValue(const RewardsSet& value) {
    if(!value.sim_)return null_ref("rewards_set");
    return pod(value.st());
}
ExecutionValue FreezeExecutionValue(LocKey value) {
    JsonWriter w;w.begin_object().field("kind","loc_key").field("hash",uint64_t(value.hash)).field("text",value.str()).end_object();return {std::move(w.str())};
}
ExecutionValue FreezeExecutionValue(CardKeywordSet value) {
    JsonWriter w;w.begin_object().field("kind","card_keyword_set").field("bits",value.bits).end_object();return {std::move(w.str())};
}
ExecutionValue FreezeExecutionValue(RoomTypeSet value) {
    JsonWriter w;w.begin_object().field("kind","room_type_set").field("bits",uint64_t(value.bits)).end_object();return {std::move(w.str())};
}
ExecutionValue FreezeExecutionValue(Merchant::EntryRef value) {
    Encoder w("merchant_entry_ref");w.field("entry_kind",value.kind);w.field("index",value.index);return w.finish();
}
}
