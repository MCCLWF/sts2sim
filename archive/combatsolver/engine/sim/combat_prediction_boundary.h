// Execution-only boundary used by combat search. It is never part of GameState
// or an ordinary protocol-v2 snapshot.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sts2 {

enum class CombatPredictionOutcome : uint8_t { Victory, Defeat };

struct CombatPredictionTerminalStamp {
    int32_t player_turn = 0;
    CombatPredictionOutcome outcome = CombatPredictionOutcome::Victory;
    friend bool operator==(const CombatPredictionTerminalStamp&, const CombatPredictionTerminalStamp&) = default;
};

// Owned game execution facts, separate from ordinary gameplay state.
// Consumers derive their strategy/history metadata from this partial trace.
enum class CombatPredictionEventKind : uint8_t {
    CardPlayStarted, CardPlayFinished, DamageReceived, CreatureAttacked,
    CardDrawn, CardDrawResolved, CardGenerated, CardGenerationResolved,
    OrbChanneled, CardEffectStarted,
};

// The source is an execution context; damage props alone do not identify it.
enum class CombatPredictionDamageSourceKind : uint8_t {
    Unknown, Card, Potion, Power, Relic, Poison, Thorns, Orb, MonsterMove,
};

// Exact game DamageResult values, stored without Sim-backed model handles.
// Creature pool refs remain valid in the owned combat snapshot even when a
// creature has been removed from the active roster.
struct CombatPredictionHitResult {
    uint16_t receiver = 0;
    int32_t props = 0;
    int32_t blocked = 0;
    int32_t unblocked = 0;
    int32_t overkill = 0;
    bool block_broken = false;
    bool fully_blocked = false;
    bool target_killed = false;
    friend bool operator==(const CombatPredictionHitResult&, const CombatPredictionHitResult&) = default;
};

struct CombatPredictionCardFacts {
    uint16_t owner = 0;
    int32_t upgrade = 0;
    uint8_t type = 0;
    friend bool operator==(const CombatPredictionCardFacts&, const CombatPredictionCardFacts&) = default;
};

struct CombatPredictionPlayFacts {
    uint16_t target = 0;
    uint8_t result_pile = 0;
    int32_t energy_spent = 0;
    int32_t energy_value = 0;
    int32_t stars_spent = 0;
    int32_t star_value = 0;
    bool auto_play = false;
    int32_t play_index = 0;
    int32_t play_count = 1;
    uint8_t target_type = 0;
    friend bool operator==(const CombatPredictionPlayFacts&, const CombatPredictionPlayFacts&) = default;
};

struct CombatPredictionEvent {
    CombatPredictionEventKind kind = CombatPredictionEventKind::CardPlayStarted;
    uint16_t card = 0;
    uint16_t card_id = 0;
    uint16_t owner = 0;
    uint16_t creature = 0;
    uint16_t dealer = 0;
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
    std::vector<CombatPredictionHitResult> hit_results;
    std::optional<CombatPredictionCardFacts> card_facts;
    std::optional<CombatPredictionPlayFacts> play_facts;
    CombatPredictionDamageSourceKind damage_source_kind = CombatPredictionDamageSourceKind::Unknown;
    std::string damage_source_id;
    friend bool operator==(const CombatPredictionEvent&, const CombatPredictionEvent&) = default;
};

struct CombatPredictionHistoryCounters {
    int32_t finished_plays = 0;
    int32_t ethereal_plays = 0;
    int32_t lightning_channels = 0;
    int32_t unblocked_hits_received = 0;
    int32_t cards_drawn = 0;
    int32_t cards_generated = 0;
    friend bool operator==(const CombatPredictionHistoryCounters&, const CombatPredictionHistoryCounters&) = default;
};

struct CombatPredictionHistoryTrace {
    std::vector<CombatPredictionEvent> events;
    CombatPredictionHistoryCounters counters;

    int32_t Record(const CombatPredictionEvent& event) {
        const int32_t index = static_cast<int32_t>(events.size());
        events.push_back(event);
        switch (event.kind) {
        case CombatPredictionEventKind::CardPlayFinished:
            ++counters.finished_plays;
            if (event.owner == 1 && event.flag) ++counters.ethereal_plays;
            break;
        case CombatPredictionEventKind::DamageReceived:
            if (event.creature == 1 && event.unblocked > 0) ++counters.unblocked_hits_received;
            break;
        case CombatPredictionEventKind::CardDrawn:
            if (event.owner == 1) ++counters.cards_drawn;
            break;
        case CombatPredictionEventKind::CardGenerated:
            if (event.owner == 1) ++counters.cards_generated;
            break;
        case CombatPredictionEventKind::OrbChanneled:
            if (event.owner == 1 && event.flag) ++counters.lightning_channels;
            break;
        default: break;
        }
        return index;
    }
    friend bool operator==(const CombatPredictionHistoryTrace&, const CombatPredictionHistoryTrace&) = default;
};

// Factual power mutations, independent of any search algorithm. Amount
// changes may happen before attachment; removal does not change Amount.
enum class CombatPredictionPowerEventKind : uint8_t { AmountChanged, Attached, Removed };
struct CombatPredictionPowerEvent {
    CombatPredictionPowerEventKind kind = CombatPredictionPowerEventKind::AmountChanged;
    uint16_t power = 0;
    uint16_t owner = 0;
    uint32_t owner_combat_id = 0;
    uint16_t power_id = 0;
    int32_t previous_amount = 0;
    int32_t amount = 0;
    bool attached = false;
    friend bool operator==(const CombatPredictionPowerEvent&, const CombatPredictionPowerEvent&) = default;
};

struct CombatPredictionBoundary {
    std::optional<CombatPredictionTerminalStamp> terminal;
    CombatPredictionHistoryTrace history;
    std::vector<CombatPredictionPowerEvent> power_events;
    friend bool operator==(const CombatPredictionBoundary&, const CombatPredictionBoundary&) = default;
};

}  // namespace sts2
