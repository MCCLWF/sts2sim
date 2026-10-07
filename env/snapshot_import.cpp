#include "env/env.h"
#include "env/snapshot_json.h"
#include "env/snapshot_internal.h"
#include "env/operation_json.h"
#include "engine/sim/state_fields.generated.h"
#include "core/xxhash64.h"
#include "game/Combat/CombatManager.h"
#include "env/portable_registry.h"
#include "env_build_identity.h"

#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
#include <type_traits>

namespace sts2 {
namespace snapshot_detail {
const std::string& runtime_id() {
    // Transport identity only; these clocks never feed game state or game RNG.
    static const std::string identity = [] {
        const uint64_t clocks[] = {
            static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()),
            static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()),
            static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&runtime_id)) };
        return std::to_string(xxhash64(clocks, sizeof(clocks), 0x73747332656e76ULL)) + "-"
            + std::to_string(xxhash64(clocks, sizeof(clocks), 0x736e617073686f74ULL));
    }();
    return identity;
}

static void object_size(const Json& json, size_t count) {
    if (json.kind != Json::Object || json.object.size() != count)
        throw GameError("snapshot: missing or unexpected object fields");
}
template<class T> bool valid_enum(T v) {
    if constexpr (requires { valid_snapshot_flags(v); }) return valid_snapshot_flags(v);
    else if constexpr (requires { T::Count; }) return static_cast<std::underlying_type_t<T>>(v) < static_cast<std::underlying_type_t<T>>(T::Count);
    else if constexpr (std::is_same_v<T, RunPhase>) return v >= RunPhase::None && v <= RunPhase::Ended;
    else if constexpr (std::is_same_v<T, HpDisplay>)
        return v == HpDisplay::Normal || v == HpDisplay::InfiniteWithNumbers || v == HpDisplay::InfiniteWithoutNumbers;
    else if constexpr (std::is_same_v<T, RestSiteOptionId>) return v <= RestSiteOptionId::Kindle;
    else return !name(v).empty();
}

class Reader {
public:
    explicit Reader(const Json& json, SnapshotReadBudget& resource_budget) : node(json), budget(resource_budget) {}
    template<class T> void value(T& v) const {
        if constexpr (std::is_same_v<T, bool>) {
            if (node.kind != Json::Bool) throw GameError("snapshot: expected bool");
            v = node.boolean;
        } else if constexpr (std::is_enum_v<T>) {
            using U = std::underlying_type_t<T>;
            v = static_cast<T>(node.integer<U>());
            if (!valid_enum(v)) throw GameError("snapshot: unknown enum value");
        } else if constexpr (std::is_integral_v<T>) v = node.integer<T>();
        else if constexpr (std::is_same_v<T, float>) {
            object_size(node, 1); v = std::bit_cast<float>(node.at("f32_bits").integer<uint32_t>());
            if (!std::isfinite(v)) throw GameError("snapshot: nonfinite float");
        } else if constexpr (std::is_same_v<T, double>) {
            object_size(node, 1); v = std::bit_cast<double>(node.at("f64_bits").integer<uint64_t>());
            if (!std::isfinite(v)) throw GameError("snapshot: nonfinite double");
        } else {
            size_t count = 0;
            visit_named_state_fields(v, [&](const char* name, auto& field) { member(name, field); ++count; });
            object_size(node, count);
        }
    }
    template<class T> void member(const char* name, T& v) const {
        try { Reader(node.at(name), budget).value(v); }
        catch (const GameError& error) { throw GameError(std::string(name) + ": " + error.what()); }
    }
    template<class T, size_t N> void value(T (&v)[N]) const {
        array_size(N); for (size_t i = 0; i < N; ++i) Reader(node.array[i], budget).value(v[i]);
    }
    template<class T, size_t N> void value(std::array<T, N>& v) const {
        array_size(N); for (size_t i = 0; i < N; ++i) Reader(node.array[i], budget).value(v[i]);
    }
    template<class T, size_t N> void value(StaticVec<T, N>& v) const {
        array_limit(StaticVec<T, N>::max_size()); v.clear();
        if (node.array.size() > v.capacity()) budget.allocation(node.array.size(), 2 * sizeof(T));
        v.reserve(node.array.size());
        for (const Json& item : node.array) { T decoded{}; Reader(item, budget).value(decoded); v.push_back(decoded); }
    }
    template<class T> void value(std::vector<T>& v) const {
        array_limit(INT32_MAX); v.clear();
        budget.allocation(node.array.size(), sizeof(T));
        v.reserve(node.array.size());
        for (const Json& item : node.array) { T decoded{}; Reader(item, budget).value(decoded); v.push_back(decoded); }
    }
    void value(std::string& v) const {
        if (node.kind != Json::String) throw GameError("snapshot: expected string");
        v = node.text;
    }
    void value(Dec& v) const {
        object_size(node, 1); uint32_t words[4]{}; Reader(node.at("decimal_bits"), budget).value(words);
        if ((words[3] & 0x7f00ffffU) != 0 || ((words[3] >> 16) & 0xff) > 28)
            throw GameError("snapshot: invalid decimal flags");
        v = Dec::from_bits(words[0], words[1], words[2], words[3]);
    }
    void value(Rng& v) const {
        object_size(node, 5);
        member("s0", v.random.s0); member("s1", v.random.s1);
        member("s2", v.random.s2); member("s3", v.random.s3); member("counter", v.counter);
        if (v.counter < 0) throw GameError("snapshot: negative RNG counter");
        // All-zero is valid for an unused/default stream; active stream checks
        // belong to combat-state validation, not this generic storage decoder.
    }
    void value(RunRngSet& v) const { rng_set(v); }
    void value(PlayerRngSet& v) const { rng_set(v); }
    template<class T, uint16_t N> void value(Pool<T, N>& v) const {
        object_size(node, 4);
        const uint32_t capacity = node.at("capacity").integer<uint32_t>();
        const Json& slots = node.at("slots"); Reader(slots, budget).array_limit(capacity);
        const uint32_t count = node.at("count").integer<uint32_t>();
        const uint32_t next = node.at("next").integer<uint32_t>();
        if (capacity > v.max_size() || count != slots.array.size() || count > capacity
            || next < 1 || next > uint64_t{capacity} + 1) throw GameError("snapshot: invalid pool metadata");
        budget.allocation(size_t{capacity} + 1, v.slot_storage_bytes());
        v.load_snapshot(capacity, node.at("next").integer<uint32_t>(), node.at("count").integer<uint32_t>(), [&](auto add) {
            for (const Json& slot : slots.array) {
                object_size(slot, 2); T item{}; Reader(slot.at("value"), budget).value(item);
                add(slot.at("index").integer<uint32_t>(), item);
            }
        });
    }
    template<class T, size_t N> void value(DotnetSet<T, N>& v) const {
        object_size(node, 5);
        if (node.at("capacity").integer<size_t>() != N) throw GameError("snapshot: set capacity mismatch");
        const Json& slots = node.at("slots"); Reader(slots, budget).array_limit(N);
        v.load_snapshot(node.at("count").integer<int>(), node.at("free_list").integer<int>(),
            node.at("free_count").integer<int>(), [&](auto add) {
                for (const Json& slot : slots.array) {
                    object_size(slot, 3); bool used{}; Reader(slot.at("used"), budget).value(used);
                    T item{}; int8_t next = -1;
                    if (used) Reader(slot.at("value"), budget).value(item);
                    else Reader(slot.at("next_free"), budget).value(next);
                    add(slot.at("index").integer<int>(), used, item, next);
                }
            });
    }
    void value(Action& v) const {
        object_size(node, 4); uint8_t kind{}; member("kind", kind);
        if (kind > Action::Resume) throw GameError("snapshot: unknown action kind");
        v.kind = static_cast<Action::Kind>(kind);
        member("index", v.index); member("target", v.target); member("coord", v.coord);
    }
    void value(Operation& v) const { v = ParseOperationDocument(node); }
    void value(NeedBoundary& v) const {
        object_size(node, 2); uint8_t kind{}; member("kind", kind);
        if (kind > 1) throw GameError("snapshot: unknown execution boundary");
        v.kind = static_cast<ExecutionBoundaryKind>(kind); member("ordinal", v.ordinal);
        if (!v.ordinal) throw GameError("snapshot: invalid boundary ordinal");
    }
    void value(ChoiceRequest& v) const {
        object_size(node, 11); uint8_t kind{}; member("kind", kind);
        if (kind > 2) throw GameError("snapshot: unknown choice kind");
        v.kind = static_cast<ChoiceKind>(kind);
        member("option_count", v.option_count); member("min_select", v.min_select); member("max_select", v.max_select);
        member("prompt", v.prompt); member("cards", v.cards); member("subset", v.subset);
        member("selected", v.selected); member("can_skip", v.can_skip); member("source", v.source);
        member("options", v.options);
        if (v.option_count < 0 || v.option_count > INT32_MAX || v.min_select < 0 || v.max_select < v.min_select)
            throw GameError("snapshot: invalid choice counts");
        for (int selected : v.selected)
            if (selected < 0 || selected >= v.option_count) throw GameError("snapshot: invalid selected choice index");
        if (v.options.size() != static_cast<size_t>(v.option_count))
            throw GameError("snapshot: incomplete choice descriptions");
        std::vector<uint32_t> flattened;
        for (const auto& option : v.options)
            flattened.insert(flattened.end(), option.cards.begin(), option.cards.end());
        if (flattened != v.cards) throw GameError("snapshot: inconsistent offered cards");
    }
    void value(ChoiceOption& v) const {
        object_size(node, 5); uint8_t kind{}; member("kind", kind);
        if (kind > static_cast<uint8_t>(ChoiceOptionKind::Proceed))
            throw GameError("snapshot: unknown choice option kind");
        v.kind = static_cast<ChoiceOptionKind>(kind);
        member("key", v.key); member("cards", v.cards); member("reward", v.reward);
        member("enabled", v.enabled);
        const bool card = v.kind == ChoiceOptionKind::Card;
        const bool bundle = v.kind == ChoiceOptionKind::Bundle;
        const bool reward = v.kind == ChoiceOptionKind::Reward;
        const bool alternative = v.kind == ChoiceOptionKind::Alternative;
        if ((card && v.cards.size() != 1) || (!card && !bundle && !v.cards.empty())
            || (reward != (v.reward != 0)) || (alternative != !v.key.empty()))
            throw GameError("snapshot: invalid choice option payload");
        for (uint32_t ref : v.cards)
            if (ref == 0 || ref > Pool<CardState, kCardPool>::max_size()) throw GameError("snapshot: invalid choice card reference");
        if (v.reward > Pool<RewardState, kRewardPool>::max_size()) throw GameError("snapshot: invalid choice reward reference");
    }
    template<class T> void value(std::optional<T>& v) const {
        if (node.is_null()) v.reset(); else { v.emplace(); value(*v); }
    }
private:
    void array_size(size_t size) const {
        if (node.kind != Json::Array || node.array.size() != size) throw GameError("snapshot: array size mismatch");
    }
    void array_limit(size_t size) const {
        if (node.kind != Json::Array || node.array.size() > size) throw GameError("snapshot: array capacity exceeded");
    }
    template<class T> void rng_set(T& v) const { object_size(node, 2); member("seed", v.seed); member("rngs", v.rngs); }
    const Json& node;
    SnapshotReadBudget& budget;
};
}  // namespace snapshot_detail

std::unique_ptr<Env> Env::ImportSnapshotJson(std::string_view input) {
    using namespace snapshot_detail;
    SnapshotReadBudget budget;
    const Json document = Parser(input).parse();
    if(document.kind==Json::Object && document.at("format").kind==Json::String
        && document.at("format").text=="sts2env.portable_snapshot") {
        object_size(document,7);
        if(document.at("version").integer<int>()!=1
            || document.at("native_build_sha256").kind!=Json::String
            || document.at("native_build_sha256").text!=kNativeBuildSha256)
            throw GameError("portable snapshot: incompatible version or native build");
        if(document.at("native_runtime_abi").kind!=Json::String
            || document.at("native_runtime_abi").text!=native_runtime_abi())
            throw GameError("portable snapshot: incompatible native math runtime or floating point environment");
        const auto& logical=document.at("logical_state"); const auto& dictionary=document.at("registry");
        if(document.at("wire_hash").integer<uint64_t>()!=portable_wire_hash(portable_payload(logical,dictionary)))
            throw GameError("portable snapshot: wire hash mismatch");
        object_size(logical,18);
        auto equals=[&](const char* key,std::string_view expected) {
            if(logical.at(key).kind!=Json::String || logical.at(key).text!=expected)
                throw GameError(std::string("portable snapshot: incompatible ")+key);
        };
        equals("format","sts2env.logical_state"); equals("schema_sha256",kStateSchemaSha256);
        equals("registry_scope","portable_canonical"); equals("runtime_id","portable");
        if(logical.at("version").integer<int>()!=4 || logical.at("model_id_hash").integer<uint32_t>()!=kModelIdHash)
            throw GameError("portable snapshot: incompatible logical version or model IDs");
        auto env=std::make_unique<Env>();
        Reader(logical.at("state"), budget).value(*env->g_);
        Reader(logical.at("pending"), budget).value(env->pending_);
        Reader(logical.at("pending_operation"), budget).value(env->pending_operation_);
        Reader(logical.at("boundary"), budget).value(env->boundary_);
        Reader(logical.at("boundary_skipped"), budget).value(env->boundary_skipped_);
        Reader(logical.at("answers"), budget).value(env->answers_);
        Reader(logical.at("choice"), budget).value(env->choice_);
        if(!logical.at("replay_base").is_null()) {
            auto base=std::make_shared<GameState>(); Reader(logical.at("replay_base"), budget).value(*base); env->base_=std::move(base);
        }
        if(logical.at("state_hash").is_null() || logical.at("state_hash").integer<uint64_t>()!=env->Hash())
            throw GameError("portable snapshot: canonical state hash mismatch");
        if(logical.at("decision").integer<int>()!=static_cast<int>(env->decision().kind))
            throw GameError("portable snapshot: decision does not match state");
        auto registry=read_portable_registry(dictionary);
        validate_portable_registry_usage(*env->g_,env->base_.get(),registry);
        if(env->pending_operation_ && env->base_) {
            Env root; *root.g_=*env->base_;
            if(root.Hash()!=env->pending_operation_->source_hash)
                throw GameError("portable snapshot: stale canonical operation source");
        }
        install_portable_registry(registry);
        remap_portable_state(*env->g_,registry);
        if(env->base_) {
            auto local=std::make_shared<GameState>(*env->base_); remap_portable_state(*local,registry); env->base_=std::move(local);
            if(env->pending_operation_) { Env root; *root.g_=*env->base_; env->pending_operation_->source_hash=root.Hash(); }
        }
        // Use the existing strict v4 validator and full stationary-prefix
        // proof after resolving identities. Never resume directly from decoded
        // partially executed state. Preserve the caller's RNG stream metadata.
        auto local=Parser(env->ExportSnapshotJson()).parse();
        local.object.at("run_rng_streams")=logical.at("run_rng_streams");
        local.object.at("player_rng_streams")=logical.at("player_rng_streams");
        // Keep unexpected pending replay_base even at an inactive decision so
        // the v4 validator rejects it instead of dropping untrusted metadata.
        if(env->base_ && !env->choice_ && !env->boundary_) local.object.at("replay_base")=logical.at("replay_base");
        JsonWriter writer; write_snapshot_json(writer,local);
        return ImportSnapshotJson(writer.str());
    }
    object_size(document, 18);
    auto text_equals = [&](const char* key, std::string_view expected) {
        const Json& value = document.at(key);
        if (value.kind != Json::String || value.text != expected) throw GameError(std::string("snapshot: incompatible ") + key);
    };
    text_equals("format", "sts2env.logical_state");
    if (document.at("version").integer<int>() != 4) throw GameError("snapshot: incompatible version");
    text_equals("schema_sha256", kStateSchemaSha256);
    text_equals("registry_scope", "loaded_native_runtime"); text_equals("runtime_id", runtime_id());
    if (document.at("model_id_hash").integer<uint32_t>() != kModelIdHash) throw GameError("snapshot: incompatible model ID hash");
    auto check_streams = [&](const char* key, const auto& names) {
        const Json& streams = document.at(key);
        if (streams.kind != Json::Array || streams.array.size() != names.size()) throw GameError("snapshot: RNG streams missing");
        for (size_t i = 0; i < names.size(); ++i)
            if (streams.array[i].kind != Json::String || streams.array[i].text != names[i]) throw GameError("snapshot: RNG stream order mismatch");
    };
    check_streams("run_rng_streams", kRunRngNames); check_streams("player_rng_streams", kPlayerRngNames);
    auto env = std::make_unique<Env>();
    Reader(document.at("state"), budget).value(*env->g_);
    Reader(document.at("pending"), budget).value(env->pending_);
    Reader(document.at("pending_operation"), budget).value(env->pending_operation_);
    Reader(document.at("boundary"), budget).value(env->boundary_);
    Reader(document.at("boundary_skipped"), budget).value(env->boundary_skipped_);
    Reader(document.at("answers"), budget).value(env->answers_);
    Reader(document.at("choice"), budget).value(env->choice_);
    const Json& base = document.at("replay_base");
    auto stable_boundary = [](Env& candidate, bool starting_run) {
        const GameState& state = candidate.state();
        validate_game_state(state);
        if (!state.run.profile.full_veteran_compatible || !state.run.player.profile.full_veteran_compatible)
            throw UnsupportedOperation("snapshot: only fully unlocked veteran profiles are supported");
        if (state.run.phase == RunPhase::Combat) {
            if (!state.combat.is_in_progress || state.player_combat.phase != PlayerTurnPhase::Play
                || state.combat.current_side != CombatSide::Player
                || state.combat.executing_card_or_potion_effect != 0)
                throw GameError("snapshot: root is not at a player combat decision boundary");
            candidate.ValidateCombatRoot();
        } else {
            if (state.combat.is_in_progress)
                throw GameError("snapshot: noncombat decision has active combat");
            if (state.run.phase == RunPhase::None && !starting_run)
                throw GameError("snapshot: run has not reached a decision");
        }
    };
    validate_game_state(*env->g_);
    if (!env->g_->run.profile.full_veteran_compatible || !env->g_->run.player.profile.full_veteran_compatible)
        throw UnsupportedOperation("snapshot: only fully unlocked veteran profiles are supported");
    if (env->choice_ && env->boundary_) throw GameError("snapshot: both choice and boundary are pending");
    if (env->choice_ || env->boundary_) {
        if (env->pending_.has_value() == env->pending_operation_.has_value()
            || base.is_null() || document.at("state_hash").is_null())
            throw GameError("snapshot: incomplete choice replay state");
        if (env->pending_ && (env->pending_->kind == Action::Answer || env->pending_->kind == Action::Resume))
            throw GameError("snapshot: choice base cannot be another answer");
        auto replay = std::make_shared<GameState>();
        Reader(base, budget).value(*replay);
        auto proof = std::make_unique<Env>();
        *proof->g_ = *replay;
        stable_boundary(*proof, env->pending_ && env->pending_->kind == Action::StartRun);
        if (env->pending_operation_) ValidateOperation(*proof, *env->pending_operation_);
        if (env->boundary_ && (!env->pending_operation_
            || !(env->pending_operation_->stop_mask & execution_boundary_bit(env->boundary_->kind))
            || env->boundary_->ordinal <= env->boundary_skipped_))
            throw GameError("snapshot: inconsistent boundary control");
        if (env->boundary_skipped_ && !env->pending_operation_)
            throw GameError("snapshot: boundary replay requires native operation");
        env->base_ = std::move(replay);
    } else {
        if (env->pending_ || env->pending_operation_ || !base.is_null())
            throw GameError("snapshot: inactive pending replay state");
        if (env->boundary_skipped_ || !env->answers_.empty())
            throw GameError("snapshot: inactive replay prefix");
        stable_boundary(*env, false);
    }
    if (document.at("decision").integer<int>() != static_cast<int>(env->decision().kind))
        throw GameError("snapshot: decision does not match state");
    if (env->g_->combat.is_in_progress) env->ValidateCombatRoot();
    if (!document.at("state_hash").is_null() && document.at("state_hash").integer<uint64_t>() != env->Hash())
        throw GameError("snapshot: state hash mismatch");
    // All effects, including event/reward/shop choices, reconstruct from their
    // stationary base and answers. No client coroutine or search history is used.
    if (env->choice_ || env->boundary_) {
        auto replay = env->Clone();
        replay->Run(true);
        if (replay->Hash() != env->Hash()) throw GameError("snapshot: inconsistent choice replay");
    }
    return env;
}
}  // namespace sts2
