// Optional execution observation. Attaching this trace never changes game rules
// or the ordinary snapshot/choice protocol.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include <string_view>
#include "engine/sim/errors.h"
#include "core/rng_observer.h"

namespace sts2 {

// Owned game execution facts, separate from ordinary gameplay state.
// Consumers derive their strategy/history metadata from this partial trace.
enum class ExecutionEventKind : uint8_t {
    CardPlayStarted, CardPlayFinished, DamageReceived, CreatureAttacked,
    CardDrawn, CardDrawResolved, CardGenerated, CardGenerationResolved,
    OrbChanneled, CardEffectStarted, DamageResolved,
};

// The source is an execution context; damage props alone do not identify it.
enum class ExecutionDamageSourceKind : uint8_t {
    Unknown, Card, Potion, Power, Relic, Poison, Thorns, Orb, MonsterMove,
};

// Exact game DamageResult values, stored without Sim-backed model handles.
// Creature pool refs remain valid in the owned combat snapshot even when a
// creature has been removed from the active roster.
struct ExecutionHitResult {
    uint32_t receiver = 0;
    int32_t props = 0;
    int32_t blocked = 0;
    int32_t unblocked = 0;
    int32_t overkill = 0;
    bool block_broken = false;
    bool fully_blocked = false;
    bool target_killed = false;
    friend bool operator==(const ExecutionHitResult&, const ExecutionHitResult&) = default;
};

struct ExecutionCardFacts {
    uint32_t owner = 0;
    int32_t upgrade = 0;
    uint8_t type = 0;
    friend bool operator==(const ExecutionCardFacts&, const ExecutionCardFacts&) = default;
};

struct ExecutionPlayFacts {
    uint32_t target = 0;
    uint8_t result_pile = 0;
    int32_t energy_spent = 0;
    int32_t energy_value = 0;
    int32_t stars_spent = 0;
    int32_t star_value = 0;
    bool auto_play = false;
    int32_t play_index = 0;
    int32_t play_count = 1;
    uint8_t target_type = 0;
    friend bool operator==(const ExecutionPlayFacts&, const ExecutionPlayFacts&) = default;
};

struct ExecutionEvent {
    ExecutionEventKind kind = ExecutionEventKind::CardPlayStarted;
    uint32_t card = 0;
    uint16_t card_id = 0;
    uint32_t owner = 0;
    uint32_t creature = 0;
    uint32_t dealer = 0;
    int32_t blocked = 0;
    int32_t unblocked = 0;
    int32_t overkill = 0;
    int32_t original_index = -1;
    int32_t hit_count = 0;
    bool flag = false; // ethereal on finished play; from-hand on draw; lightning on channel
    int32_t props = 0;
    bool block_broken = false;
    bool fully_blocked = false;
    bool target_killed = false;
    std::vector<ExecutionHitResult> hit_results;
    std::optional<ExecutionCardFacts> card_facts;
    std::optional<ExecutionPlayFacts> play_facts;
    ExecutionDamageSourceKind damage_source_kind = ExecutionDamageSourceKind::Unknown;
    std::string damage_source_id;
    uint64_t sequence = 0;
    int32_t frame = -1;
    uint32_t creature_combat_id = UINT32_MAX;
    uint32_t dealer_combat_id = UINT32_MAX;
    friend bool operator==(const ExecutionEvent&, const ExecutionEvent&) = default;
};

// Factual power mutations, independent of any search algorithm. Amount
// changes may happen before attachment; removal does not change Amount.
enum class ExecutionPowerEventKind : uint8_t { AmountChanged, Attached, Removed };
struct ExecutionPowerEvent {
    ExecutionPowerEventKind kind = ExecutionPowerEventKind::AmountChanged;
    uint32_t power = 0;
    uint32_t owner = 0;
    uint32_t owner_combat_id = 0;
    uint16_t power_id = 0;
    int32_t previous_amount = 0;
    int32_t amount = 0;
    bool attached = false;
    uint64_t sequence = 0;
    int32_t frame = -1;
    friend bool operator==(const ExecutionPowerEvent&, const ExecutionPowerEvent&) = default;
};

struct ExecutionValue {
    std::string json;
    bool complete=true;
};
struct ExecutionArgument {
    std::string name;
    std::string declared_type;
    uint8_t flags=0; // native metadata: Out=1, ByRef=2, In=4
    ExecutionValue value;
};
struct ExecutionFrame {
    int32_t parent = -1;
    ExecutionDamageSourceKind source_kind = ExecutionDamageSourceKind::Unknown;
    std::string source_id;
    std::string operation;
    std::string kind="operation";
    std::string source_model_kind;
    std::string source_model_id;
    uint32_t source_ref=0;
    bool source_is_override=false;
    bool source_is_canonical=false;
    bool source_is_canonical_known=false;
    std::string source_ref_kind;
    uint64_t sequence=0;
    uint64_t finished_sequence=0;
    bool finished=false;
    bool unwound=false;
    std::vector<ExecutionArgument> parameters_before;
    std::vector<ExecutionArgument> parameters_after;
    std::optional<ExecutionValue> return_value;
    std::string declared_return_type;
    bool method_values_observed=false;
    bool method_returned=false;
};

struct ExecutionRngCall {
    std::string operation;
    std::string scope;
    std::string stream;
    std::string owner_model_id;
    uint32_t owner_ref=0;
    uint32_t owner_combat_id=UINT32_MAX;
    int32_t frame=-1;
    int32_t parent_call=-1;
    std::vector<RngTraceValue> parameters;
    std::vector<uint32_t> weight_bits;
    RngTraceState before;
    RngTraceState after;
    RngTraceValue result;
    uint64_t sequence=0;
    uint64_t finished_sequence=0;
    bool finished=false;
    friend bool operator==(const ExecutionRngCall&,const ExecutionRngCall&)=default;
};

// Observation data only. No terminal override, policy counters or gameplay
// state. Events freeze their inputs before ordinary cleanup can retire objects.
struct ExecutionTrace {
    std::vector<ExecutionFrame> frames;
    std::vector<ExecutionEvent> events;
    std::vector<ExecutionPowerEvent> power_events;
    std::vector<ExecutionRngCall> rng_calls;
    bool collect_rng=false;
    bool collect_causal=false;
    size_t limit = 16384;
    int32_t current_frame = -1;
    uint64_t next_sequence = 0;

    void CheckLimit(size_t additional = 1) const {
        if (additional > limit || frames.size() + events.size() + power_events.size() + rng_calls.size() > limit - additional)
            throw TraceLimitExceeded("execution trace capacity reached");
    }
    int32_t Enter(std::string_view operation, ExecutionDamageSourceKind source_kind,
                  std::string_view source_id) {
        ExecutionFrame frame;frame.operation=operation;frame.source_kind=source_kind;frame.source_id=source_id;
        return Enter(std::move(frame));
    }
    int32_t Enter(ExecutionFrame frame) {
        CheckLimit();
        const int32_t index = static_cast<int32_t>(frames.size());
        frame.parent=current_frame;frame.sequence=next_sequence;
        frames.push_back(std::move(frame));++next_sequence;
        current_frame = index;
        return index;
    }
    void Leave(int32_t index,bool unwound=false) noexcept {
        auto& frame=frames[static_cast<size_t>(index)];
        if(!frame.finished) {
            frame.finished_sequence=next_sequence++;frame.finished=true;frame.unwound=unwound;
        }
        current_frame=frame.parent;
    }
    int32_t Record(ExecutionEvent event) {
        CheckLimit();
        const int32_t index = static_cast<int32_t>(events.size());
        event.sequence = next_sequence++;
        event.frame = current_frame;
        events.push_back(std::move(event));
        return index;
    }
    void RecordPower(ExecutionPowerEvent event) {
        CheckLimit();
        event.sequence = next_sequence++;
        event.frame = current_frame;
        power_events.push_back(event);
    }
};

}  // namespace sts2
