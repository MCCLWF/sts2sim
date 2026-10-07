// Versioned complete logical state for privileged transport and diagnostics.
// No raw layout/padding, pointers, inactive vector tails or freed pool values.
#include "env/env.h"

#include <bit>
#include <type_traits>

#include "engine/json.h"
#include "engine/sim/state_fields.generated.h"
#include "env/snapshot_internal.h"
#include "env/portable_registry.h"

namespace sts2 {
namespace {

class SnapshotWriter {
public:
    explicit SnapshotWriter(JsonWriter& writer) : w(writer) {}

    template<class T> void value(const T& v) {
        if constexpr (std::is_same_v<T, bool>) w.value(v);
        else if constexpr (std::is_enum_v<T>) value(static_cast<std::underlying_type_t<T>>(v));
        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) w.value(static_cast<int64_t>(v));
        else if constexpr (std::is_integral_v<T>) w.value(static_cast<uint64_t>(v));
        else if constexpr (std::is_same_v<T, float>)
            w.begin_object().field("f32_bits", static_cast<uint64_t>(std::bit_cast<uint32_t>(v))).end_object();
        else if constexpr (std::is_same_v<T, double>)
            w.begin_object().field("f64_bits", std::bit_cast<uint64_t>(v)).end_object();
        else {
            w.begin_object();
            visit_named_state_fields(v, [this](const char* name, const auto& field) { member(name, field); });
            w.end_object();
        }
    }

    template<class T> void member(const char* name, const T& v) { w.key(name); value(v); }

    template<class T, size_t N> void value(const T (&v)[N]) { sequence(v); }
    template<class T, size_t N> void value(const std::array<T, N>& v) { sequence(v); }
    template<class T, size_t N> void value(const StaticVec<T, N>& v) { sequence(v); }
    template<class T> void value(const std::vector<T>& v) { sequence(v); }
    void value(const std::string& v) { w.value(v); }
    void value(std::string_view v) { w.value(v); }

    void value(const Dec& v) {
        w.begin_object().key("decimal_bits").begin_array();
        value(v.lo); value(v.mid); value(v.hi); value(v.flags);
        w.end_array().end_object();
    }
    void value(const Rng& v) {
        w.begin_object();
        member("s0", v.random.s0); member("s1", v.random.s1);
        member("s2", v.random.s2); member("s3", v.random.s3); member("counter", v.counter);
        w.end_object();
    }
    void value(const RunRngSet& v) { rng_set(v); }
    void value(const PlayerRngSet& v) { rng_set(v); }

    template<class T, uint16_t N> void value(const Pool<T, N>& v) {
        w.begin_object();
        member("capacity", v.capacity());
        v.visit_snapshot([&](uint32_t next, uint32_t count) {
            member("next", next); member("count", count);
            w.key("slots").begin_array();
        }, [&](uint32_t index, const T& item) {
            w.begin_object(); member("index", index); member("value", item); w.end_object();
        });
        w.end_array().end_object();
    }

    template<class T, size_t N> void value(const DotnetSet<T, N>& v) {
        w.begin_object();
        member("capacity", N);
        v.visit_snapshot([&](int count, int free_list, int free_count) {
            member("count", count); member("free_list", free_list); member("free_count", free_count);
            w.key("slots").begin_array();
        }, [&](int index, bool used, const T& item, int8_t next_free) {
            w.begin_object(); member("index", index); member("used", used);
            if (used) member("value", item);
            else member("next_free", next_free);
            w.end_object();
        });
        w.end_array().end_object();
    }

    void value(const Action& v) {
        w.begin_object();
        member("kind", v.kind); member("index", v.index); member("target", v.target); member("coord", v.coord);
        w.end_object();
    }
    void value(const Operation& v) { WriteOperation(w, v); }
    void value(const NeedBoundary& v) {
        w.begin_object(); member("kind", v.kind); member("ordinal", v.ordinal); w.end_object();
    }
    void value(const ChoiceRequest& v) {
        w.begin_object();
        member("kind", v.kind); member("option_count", v.option_count);
        member("min_select", v.min_select); member("max_select", v.max_select);
        member("prompt", v.prompt); member("cards", v.cards); member("subset", v.subset);
        member("selected", v.selected); member("can_skip", v.can_skip); member("source", v.source);
        member("options", v.options);
        w.end_object();
    }
    void value(const ChoiceOption& v) {
        w.begin_object();
        member("kind", v.kind); member("key", v.key); member("cards", v.cards); member("reward", v.reward);
        member("enabled", v.enabled);
        w.end_object();
    }
    template<class T> void value(const std::optional<T>& v) {
        if (v) value(*v); else w.null();
    }

private:
    template<class T> void sequence(const T& v) {
        w.begin_array(); for (const auto& item : v) value(item); w.end_array();
    }
    template<class T> void rng_set(const T& v) {
        w.begin_object(); member("seed", v.seed); member("rngs", v.rngs); w.end_object();
    }
    JsonWriter& w;
};

void begin_snapshot(JsonWriter& w, std::optional<uint64_t> hash, std::optional<DecisionKind> decision) {
    w.begin_object().field("format", "sts2env.logical_state").field("version", 4)
        .field("schema_sha256", kStateSchemaSha256).field("model_id_hash", static_cast<uint64_t>(kModelIdHash))
        .field("registry_scope", "loaded_native_runtime").key("state_hash");
    if (hash) w.value(*hash); else w.null();
    w.field("runtime_id", snapshot_detail::runtime_id()).key("decision");
    if (decision) w.value(static_cast<int32_t>(*decision)); else w.null();
}
}  // namespace

std::string Env::ExportSnapshotJson() const {
    if ((choice_ || boundary_) && (!base_ || (pending_.has_value() == pending_operation_.has_value())))
        throw GameError("snapshot: incomplete choice replay state");
    JsonWriter w;
    SnapshotWriter snapshot(w);
    begin_snapshot(w, Hash(), decision().kind);
    snapshot.member("run_rng_streams", kRunRngNames);
    snapshot.member("player_rng_streams", kPlayerRngNames);
    snapshot.member("state", *g_);
    snapshot.member("pending", pending_);
    snapshot.member("pending_operation", pending_operation_);
    snapshot.member("boundary", boundary_);
    snapshot.member("boundary_skipped", boundary_skipped_);
    snapshot.member("answers", answers_);
    snapshot.member("choice", choice_);
    w.key("replay_base");
    if (choice_ || boundary_) snapshot.value(*base_); else w.null();
    w.end_object();
    return w.str();
}

std::string Env::ExportPortableSnapshotJson() const {
    const GameState* base=(choice_ || boundary_) ? base_.get() : nullptr;
    auto registry=snapshot_detail::collect_portable_registry(*g_,base);
    auto canonical=Clone();
    snapshot_detail::remap_portable_state(*canonical->g_,registry);
    if(base) {
        auto copy=std::make_shared<GameState>(*base);
        snapshot_detail::remap_portable_state(*copy,registry);
        canonical->base_=std::move(copy);
        if(canonical->pending_operation_) {
            Env root; *root.g_=*canonical->base_;
            canonical->pending_operation_->source_hash=root.Hash();
        }
    }
    auto logical=snapshot_detail::Parser(canonical->ExportSnapshotJson()).parse();
    logical.object.at("registry_scope").text="portable_canonical";
    logical.object.at("runtime_id").text="portable";
    JsonWriter writer; snapshot_detail::write_portable_registry(writer,registry);
    auto dictionary=snapshot_detail::Parser(writer.str()).parse();
    auto payload=snapshot_detail::portable_payload(logical,dictionary);
    const uint64_t hash=snapshot_detail::portable_wire_hash(payload);
    payload.pop_back(); payload+=",\"wire_hash\":"+std::to_string(hash)+"}";
    return payload;
}

}  // namespace sts2
